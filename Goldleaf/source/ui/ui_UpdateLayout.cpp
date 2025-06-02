
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

#include <ui/ui_UpdateLayout.hpp>
#include <ui/ui_MainApplication.hpp>

extern ui::MainApplication::Ref g_MainApplication;
extern cfg::Settings g_Settings;
extern bool g_UpdatedNeedsRename;

namespace ui {

    namespace json {

        struct GitHubRelease {
            std::string tag_name;
        };
        
        using GitHubReleases = std::vector<GitHubRelease>;

    }

    UpdateLayout::UpdateLayout() {
        this->info_text = pu::ui::elm::TextBlock::New(150, 350, "...");
        this->info_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->info_text->SetColor(g_Settings.GetColorScheme().text);
        this->download_p_bar = pu::ui::elm::ProgressBar::New(0, 420, 1750, 60, 100.0f);
        this->download_p_bar->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->download_p_bar->SetProgressColor(g_Settings.GetColorScheme().progress_bar);
        this->download_p_bar->SetBackgroundColor(g_Settings.GetColorScheme().progress_bar_bg);
        this->Add(this->info_text);
        this->Add(this->download_p_bar);
    }

    void UpdateLayout::StartUpdateSearch() {
        g_MainApplication->LoadCommonIconMenuData(true, cfg::Strings.GetString(284), CommonIconKind::Update, cfg::Strings.GetString(302));

        ScopeGuard on_exit([&]() {
            g_MainApplication->ReturnToParentLayout();
        });

        this->download_p_bar->SetVisible(false);
        this->info_text->SetText(cfg::Strings.GetString(305));
        g_MainApplication->CallForRender();

        const auto json_data = net::RetrieveContent("https://api.github.com/repos/XorTroll/Goldleaf/releases", "application/json");
        if(json_data.empty()) {
            g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(316), { cfg::Strings.GetString(234) }, true);
            return;
        }
        json::GitHubReleases releases;
        const auto err = glz::read<PartialJsonOptions{}>(releases, json_data);
        if(err || releases.empty()) {
            g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(316), { cfg::Strings.GetString(234) }, true);
            return;
        }
        const auto last_id = releases.front().tag_name;
        if(last_id.empty()) {
            g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(316), { cfg::Strings.GetString(234) }, true);
            return;
        }
        this->info_text->SetText(cfg::Strings.GetString(306));
        g_MainApplication->CallForRender();

        const auto last_ver = Version::FromString(last_id);
        const auto cur_ver = Version::MakeVersion(GLEAF_MAJOR, GLEAF_MINOR, GLEAF_MICRO);
        if(last_ver.IsEqual(cur_ver)) {
            g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(307), { cfg::Strings.GetString(234) }, true);
        }
        else if(last_ver.IsLower(cur_ver)) {
            const auto option = g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(308), { cfg::Strings.GetString(111), cfg::Strings.GetString(18) }, true);
            if(option == 0) {
                EnsureDirectories();
                auto sd_exp = fs::GetSdCardExplorer();
                sd_exp->DeleteFile(GLEAF_PATH_TEMP_UPDATE_NRO);

                this->info_text->SetText(cfg::Strings.GetString(309));
                g_MainApplication->CallForRender();
                
                hos::LockExit();
                this->download_p_bar->SetVisible(true);
                const auto download_url = "https://github.com/XorTroll/Goldleaf/releases/download/" + last_id + "/Goldleaf.nro";
                sd_exp->DeleteFile(GLEAF_PATH_TEMP_UPDATE_NRO);
                net::RetrieveToFile(download_url, sd_exp->AbsolutePathFor(GLEAF_PATH_TEMP_UPDATE_NRO), [&](const double done, const double total) {
                    this->download_p_bar->SetMaxProgress(total);
                    this->download_p_bar->SetProgress(done);
                    g_MainApplication->CallForRender();
                });
                this->download_p_bar->SetVisible(false);
                hos::UnlockExit();

                if(sd_exp->IsFile(GLEAF_PATH_TEMP_UPDATE_NRO)) {
                    g_UpdatedNeedsRename = true;
                }
                
                g_MainApplication->CallForRender();
                g_MainApplication->ShowNotification(cfg::Strings.GetString(314) + " " + cfg::Strings.GetString(315));
            }
        }
        else if(last_ver.IsHigher(cur_ver)) {
            g_MainApplication->DisplayDialog(cfg::Strings.GetString(284), cfg::Strings.GetString(512), { cfg::Strings.GetString(234) }, true);
        }
    }

}
