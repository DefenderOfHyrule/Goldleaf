
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

#include <ui/ui_UpdateInstallLayout.hpp>
#include <ui/ui_MainApplication.hpp>
#include <hos/hos_Payload.hpp>

extern ui::MainApplication::Ref g_MainApplication;
extern cfg::Settings g_Settings;

namespace ui {

    namespace {

        constexpr size_t UpdateWorkBufferSize = 0x100000;

    }

    UpdateInstallLayout::UpdateInstallLayout() : pu::ui::Layout() {
        this->info_text = pu::ui::elm::TextBlock::New(150, 320, cfg::Strings.GetString(151));
        this->info_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->info_text->SetColor(g_Settings.GetColorScheme().text);
        this->p_bar = pu::ui::elm::ProgressBar::New(340, 360, 600, 30, 100.0f);
        this->p_bar->SetVisible(false);
        this->p_bar->SetProgressColor(g_Settings.GetColorScheme().progress_bar);
        this->p_bar->SetBackgroundColor(g_Settings.GetColorScheme().progress_bar_bg);
        this->Add(this->info_text);
        this->Add(this->p_bar);
    }

    void UpdateInstallLayout::InstallUpdate(const std::string &path, const bool with_exfat) {
        this->info_text->SetText(cfg::Strings.GetString(426));
        g_MainApplication->CallForRender();
        this->p_bar->SetVisible(true);
        hos::UnlockExit();
        auto rc = amssuSetupUpdate(nullptr, UpdateWorkBufferSize, path.c_str(), with_exfat);
        if(R_SUCCEEDED(rc)) {
            AsyncResult async_rc;
            rc = amssuRequestPrepareUpdate(&async_rc);
            if(R_SUCCEEDED(rc)) {
                auto wait_ok = false;
                while(true) {
                    rc = asyncResultWait(&async_rc, 0);
                    if(R_SUCCEEDED(rc)) {
                        rc = asyncResultGet(&async_rc);
                        wait_ok = R_SUCCEEDED(rc);
                        break;
                    }
                    else if(rc == 0xEA01) {
                        auto prepared = false;
                        amssuHasPreparedUpdate(&prepared);
                        if(prepared) {
                            wait_ok = true;
                            break;
                        }

                        // Get and show progress
                        NsSystemUpdateProgress update_progress = {};
                        rc = amssuGetPrepareUpdateProgress(&update_progress);
                        if(R_SUCCEEDED(rc)) {
                            this->p_bar->SetProgress(static_cast<double>(update_progress.current_size));
                            this->p_bar->SetMaxProgress(static_cast<double>(update_progress.total_size));
                            g_MainApplication->CallForRender();
                        }
                    }
                    else {
                        break;
                    }
                }
                if(wait_ok) {
                    this->p_bar->SetVisible(false);
                    this->info_text->SetText(cfg::Strings.GetString(427));
                    g_MainApplication->CallForRender();
                    rc = amssuApplyPreparedUpdate();
                    if(R_SUCCEEDED(rc)) {
                        g_MainApplication->DisplayDialog(cfg::Strings.GetString(424), cfg::Strings.GetString(431) + "\n" + cfg::Strings.GetString(432), { cfg::Strings.GetString(234) }, true);
                        hos::Reboot();
                    }
                    else {
                        g_MainApplication->ShowNotification(cfg::Strings.GetString(430));
                    }
                }
                else {
                    g_MainApplication->ShowNotification(cfg::Strings.GetString(429));
                }
            }
            else {
                g_MainApplication->ShowNotification(cfg::Strings.GetString(429));
            }
        }
        else {
            g_MainApplication->ShowNotification(cfg::Strings.GetString(428));
        }

        g_MainApplication->DisplayDialog(cfg::Strings.GetString(424), cfg::Strings.GetString(432), { cfg::Strings.GetString(234) }, true);
        hos::Reboot();
    }

}
