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

    Result GetAmiiboData(AmiiboData &out_data) {
        GLEAF_RC_ASSERT(nfpGetTagInfo(&g_DeviceHandle, &out_data.tag_info));
        GLEAF_RC_ASSERT(nfpGetRegisterInfo(&g_DeviceHandle, &out_data.register_info));
        GLEAF_RC_ASSERT(nfpGetCommonInfo(&g_DeviceHandle, &out_data.common_info));
        GLEAF_RC_ASSERT(nfpGetModelInfo(&g_DeviceHandle, &out_data.model_info));
        GLEAF_RC_ASSERT(nfpGetAdminInfo(&g_DeviceHandle, &out_data.admin_info));
        GLEAF_RC_ASSERT(nfpGetAll(&g_DeviceHandle, &out_data.data));
        GLEAF_RC_SUCCEED;
    }

    std::string ExportAsVirtualAmiibo(const AmiiboData &data) {
        auto sd_exp = fs::GetSdCardExplorer();

        std::string emuiibo_path = "emuiibo";
        sd_exp->CreateDirectory(emuiibo_path);
        emuiibo_path += "/amiibo";
        sd_exp->CreateDirectory(emuiibo_path);

        std::string amiibo_folder = data.register_info.amiibo_name;
        auto amiibo_path = emuiibo_path + "/" + amiibo_folder;
        auto i = 1;
        while(sd_exp->IsDirectory(amiibo_path)) {
            amiibo_folder = std::string(data.register_info.amiibo_name) + "_" + std::to_string(i);
            amiibo_path = emuiibo_path + "/" + amiibo_folder;
            i++;
        }
        sd_exp->CreateDirectory(amiibo_path);
        sd_exp->CreateFile(amiibo_path + "/amiibo.flag");

        const auto id = GetInternalAmiiboIdFromModelInfo(data.model_info);

        const auto mii_charinfo = "mii-charinfo.bin";
        sd_exp->WriteFile(amiibo_path + "/" + mii_charinfo, &data.register_info.mii, sizeof(data.register_info.mii));

        // use correct member name for legacy mii data
        const auto legacy_mii = "legacy-mii.bin";
        sd_exp->WriteFile(amiibo_path + "/" + legacy_mii, &data.data.mii, sizeof(data.data.mii));

        json::Uuid uuid;
        // use 'uuid' instead of 'uid'
        for(u32 i = 0; i < sizeof(data.tag_info.uuid); i++) {
            uuid.push_back(data.tag_info.uuid[i]);
        }

        json::Amiibo amiibo  = {
            // use separate day/month/year fields instead of date struct
            .first_write_date = { data.register_info.first_write_day, data.register_info.first_write_month, data.register_info.first_write_year },
            .last_write_date = { data.common_info.last_write_day, data.common_info.last_write_month, data.common_info.last_write_year },
            .name = data.register_info.amiibo_name,
            .version = data.common_info.version,
            .write_counter = data.common_info.write_counter,
            .id = {
                .character_variant = id.character_id.character_variant,
                .game_character_id = id.character_id.game_character_id,
                .series = id.series,
                .model_number = id.model_number,
                .figure_type = id.figure_type,
            },
            .uuid = std::move(uuid),
            .mii_charinfo_file = mii_charinfo,
        };
        GLEAF_ASSERT_TRUE(!glz::write_file_json<PartialJsonOptions{}>(amiibo, sd_exp->MakeAbsolute(amiibo_path + "/amiibo.json"), std::string{}));

        // If the amiibo has application area...
        if(data.admin_info.flags & BIT(1)) {
            const auto areas_dir = amiibo_path + "/areas";
            sd_exp->CreateDirectory(areas_dir);

            // use correct member name for access ID
            const auto area_path = areas_dir + "/" + util::FormatHex(data.admin_info.application_area_id) + ".bin";
            sd_exp->WriteFile(area_path, data.data.application_area, sizeof(data.data.application_area));

            json::AreaInfo area_info = {
                .current_area_access_id = data.admin_info.application_area_id,
                .areas = {
                    {
                        .program_id = data.admin_info.application_id,
                        .access_id = data.admin_info.application_area_id
                    }
                },
            };
            GLEAF_ASSERT_TRUE(!glz::write_file_json<PartialJsonOptions{}>(area_info, sd_exp->MakeAbsolute(amiibo_path + "/areas.json"), std::string{}));
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