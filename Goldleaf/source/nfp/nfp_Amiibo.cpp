
/*

    Goldleaf - Multipurpose homebrew tool for Nintendo Switch
    Copyright © 2018-2025 XorTroll

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <nfp/nfp_Amiibo.hpp>
#include <fs/fs_FileSystem.hpp>
#include <util/util_String.hpp>

namespace nfp {

    namespace {

        NfcDeviceHandle g_DeviceHandle;
        bool g_Initialized = false;

        inline Result GetAllImpl(NfpData *out_data) {
            return serviceDispatchIn(nfpGetServiceSession_Interface(), 200, g_DeviceHandle,
                .buffer_attrs = { SfBufferAttr_FixedSize | SfBufferAttr_HipcPointer | SfBufferAttr_Out },
                .buffers = { { out_data, sizeof(NfpData) } },
            );
        }

        constexpr size_t LegacyMiiSize = 0x60;

    }

    Result Initialize() {
        if(!g_Initialized) {
            // Note: using debug service (for genuine amiibos) since this isn't intercepted by emuiibo
            GLEAF_RC_TRY(nfpInitialize(NfpServiceType_Debug));
            GLEAF_RC_TRY(nfpListDevices(nullptr, &g_DeviceHandle, 1));
            GLEAF_RC_TRY(nfpStartDetection(&g_DeviceHandle));
            g_Initialized = true;
        }
        GLEAF_RC_SUCCEED;
    }

    bool IsReady() {
        if(!g_Initialized) {
            return false;
        }

        auto dev_state = NfpDeviceState_Unavailable;
        nfpGetDeviceState(&g_DeviceHandle, &dev_state);
        return dev_state == NfpDeviceState_TagFound;
    }

    Result Open() {
        return nfpMount(&g_DeviceHandle, NfpDeviceType_Amiibo, NfpMountTarget_All);
    }

    NfpTagInfo GetTagInfo() {
        NfpTagInfo tag_info = {};
        GLEAF_RC_ASSERT(nfpGetTagInfo(&g_DeviceHandle, &tag_info));
        return tag_info;
    }

    NfpRegisterInfo GetRegisterInfo() {
        NfpRegisterInfo reg_info = {};
        GLEAF_RC_ASSERT(nfpGetRegisterInfo(&g_DeviceHandle, &reg_info));
        return reg_info;
    }

    NfpCommonInfo GetCommonInfo() {
        NfpCommonInfo common_info = {};
        GLEAF_RC_ASSERT(nfpGetCommonInfo(&g_DeviceHandle, &common_info));
        return common_info;
    }

    NfpModelInfo GetModelInfo() {
        NfpModelInfo model_info = {};
        GLEAF_RC_ASSERT(nfpGetModelInfo(&g_DeviceHandle, &model_info));

        // Convert nfp service's amiibo ID format to emuiibo one
        model_info.amiibo_id[5] = model_info.amiibo_id[4];
        model_info.amiibo_id[4] = 0;
        model_info.amiibo_id[7] = 2;
        return model_info;
    }

    NfpData GetAll() {
        NfpData data = {};
        GLEAF_RC_ASSERT(GetAllImpl(&data));
        return data;
    }

    std::string ExportAsVirtualAmiibo(const NfpTagInfo &tag, const NfpRegisterInfo &reg, const NfpCommonInfo &common, const NfpModelInfo &model, const NfpData &all) {
        auto sd_exp = fs::GetSdCardExplorer();

        std::string emuiibo_path = "emuiibo";
        sd_exp->CreateDirectory(emuiibo_path);
        emuiibo_path += "/amiibo";
        sd_exp->CreateDirectory(emuiibo_path);

        std::string amiibo_folder = reg.amiibo_name;
        auto amiibo_path = emuiibo_path + "/" + amiibo_folder;
        auto i = 1;
        while(sd_exp->IsDirectory(amiibo_path)) {
            amiibo_folder = std::string(reg.amiibo_name) + "_" + std::to_string(i);
            amiibo_path = emuiibo_path + "/" + amiibo_folder;
            i++;
        }
        sd_exp->CreateDirectory(amiibo_path);
        sd_exp->CreateFile(amiibo_path + "/amiibo.flag");

        auto amiibo = JSON::object();
        amiibo["first_write_date"]["d"] = reg.first_write_day;
        amiibo["first_write_date"]["m"] = reg.first_write_month;
        amiibo["first_write_date"]["y"] = reg.first_write_year;

        amiibo["last_write_date"]["d"] = common.last_write_day;
        amiibo["last_write_date"]["m"] = common.last_write_month;
        amiibo["last_write_date"]["y"] = common.last_write_year;

        const auto mii_charinfo = "mii-charinfo.bin";
        amiibo["mii_charinfo_file"] = mii_charinfo;
        sd_exp->WriteFile(amiibo_path + "/" + mii_charinfo, &reg.mii, sizeof(reg.mii));

        const auto legacy_mii = "legacy-mii.bin";
        sd_exp->WriteFile(amiibo_path + "/" + legacy_mii, &all.mii, LegacyMiiSize);

        for(u32 i = 0; i < sizeof(tag.uuid); i++) {
            amiibo["uuid"][i] = tag.uuid[i];
        }

        amiibo["name"] = reg.amiibo_name;

        amiibo["version"] = common.version;

        amiibo["write_counter"] = common.write_counter;

        const auto id_ref = reinterpret_cast<const AmiiboId*>(model.amiibo_id);
        amiibo["id"]["character_variant"] = id_ref->character_id.character_variant;
        amiibo["id"]["game_character_id"] = id_ref->character_id.game_character_id;
        amiibo["id"]["series"] = id_ref->series;
        amiibo["id"]["model_number"] = __builtin_bswap16(id_ref->model_number);
        amiibo["id"]["figure_type"] = id_ref->figure_type;

        sd_exp->WriteJSON(amiibo_path + "/amiibo.json", amiibo);

        // If the amiibo has application area...
        if(all.settings_flag & BIT(5)) {
            auto area_info = JSON::object();
            area_info["current_area_access_id"] = all.access_id;
            area_info["areas"][0]["program_id"] = all.application_id;
            area_info["areas"][0]["access_id"] = all.access_id;

            sd_exp->WriteJSON(amiibo_path + "/areas.json", area_info);

            const auto areas_dir = amiibo_path + "/areas";
            sd_exp->CreateDirectory(areas_dir);

            const auto area_path = areas_dir + "/" + util::FormatHex(all.access_id) + ".bin";
            sd_exp->WriteFile(area_path, all.application_area, sizeof(all.application_area));
        }

        return amiibo_folder;
    }

    void Close() {
        nfpUnmount(&g_DeviceHandle);
    }

    void Exit() {
        if(g_Initialized) {
            nfpStopDetection(&g_DeviceHandle);
            nfpExit();
            g_Initialized = false;
        }
    }

}
