
/*

    Goldleaf - Multipurpose homebrew tool for Nintendo Switch
    Copyright (C) 2018-2023 XorTroll

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

#include <ui/ui_AmiiboDumpLayout.hpp>
#include <ui/ui_MainApplication.hpp>
#include <nfp/nfp_Amiibo.hpp>

extern ui::MainApplication::Ref g_MainApplication;
extern cfg::Settings g_Settings;

namespace ui {

    void AmiiboDumpLayout::OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        if(keys_down & HidNpadButton_B) {
            g_MainApplication->ReturnToParentLayout();
        }
    }

    AmiiboDumpLayout::AmiiboDumpLayout() : pu::ui::Layout() {
        this->info_text = pu::ui::elm::TextBlock::New(0, 0, "-");
        this->info_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->info_text->SetVerticalAlign(pu::ui::elm::VerticalAlign::Center);
        this->info_text->SetColor(g_Settings.GetColorScheme().text);
        this->Add(this->info_text);

        this->SetOnInput(std::bind(&AmiiboDumpLayout::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    }

    void AmiiboDumpLayout::StartDump() {
        g_MainApplication->LoadCommonIconMenuData(true, cfg::Strings.GetString(283), CommonIconKind::Amiibo, cfg::Strings.GetString(301));

        this->info_text->SetText(cfg::Strings.GetString(294));
        auto rc = nfp::Initialize();
        if(R_SUCCEEDED(rc)) {
            this->info_text->SetText(cfg::Strings.GetString(295));
            while(!nfp::IsReady()) {
                g_MainApplication->CallForRender();
            }
            rc = nfp::Open();
            if(R_SUCCEEDED(rc)) {
                nfp::AmiiboData data;
                GLEAF_RC_ASSERT(nfp::GetAmiiboData(data));
                const auto option = g_MainApplication->DisplayDialog(cfg::Strings.GetString(283), cfg::Strings.GetString(317) + " '" + data.register_info.amiibo_name + "'?", { cfg::Strings.GetString(111), cfg::Strings.GetString(112) }, true);
                if(option == 0) {
                    this->info_text->SetText(cfg::Strings.GetString(296) + " '" + data.register_info.amiibo_name + "' " + cfg::Strings.GetString(297));
                    const auto virtual_amiibo_folder = nfp::ExportAsVirtualAmiibo(data);
                    g_MainApplication->ShowNotification("'" + std::string(data.register_info.amiibo_name) + "' " + cfg::Strings.GetString(298) + " (" + virtual_amiibo_folder + ")");
                }
                nfp::Close();
            }
            nfp::Exit();
        }

        if(R_FAILED(rc)) {
            HandleResult(rc, cfg::Strings.GetString(456));
        }

        g_MainApplication->ReturnToParentLayout();
    }

}
