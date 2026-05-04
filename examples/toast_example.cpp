#include <ftxui_components/toast_component.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

int main()
{
    using namespace std::chrono_literals;

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    std::atomic<bool> cancel_processing{false};
    std::atomic<bool> processing{false};
    std::thread worker;

    auto content = ftxui::Renderer([&]
                                   {
                                       return ftxui::vbox({
                                                  ftxui::text("ftxui_components Toast example") | ftxui::bold,
                                                  ftxui::separator(),
                                                  ftxui::text("Press i to show an info toast."),
                                                  ftxui::text("Press e to show an error toast."),
                                                  ftxui::text("Press p to start a background progress toast."),
                                                  ftxui::text("Press Esc or q to exit."),
                                                  ftxui::separator(),
                                                  ftxui::paragraph("Toasts are rendered as an overlay using FTXUI dbox/clear_under, so they do not consume space in the main layout."),
                                                  ftxui::filler(),
                                              }) |
                                              ftxui::border;
                                   });

    ToastHostOption toast_option;
    toast_option.screen           = &screen;
    toast_option.width            = 48;
    toast_option.max_visible      = 3;
    toast_option.style.info       = ftxui::Color::Cyan;
    toast_option.style.success    = ftxui::Color::GreenLight;
    toast_option.style.warning    = ftxui::Color::YellowLight;
    toast_option.style.error      = ftxui::Color::RedLight;
    toast_option.style.background = ftxui::Color::GrayDark;

    auto toast_host = std::make_shared<ToastHostComponent>(content, toast_option);

    auto start_processing = [&]
    {
        if (processing.exchange(true))
        {
            ToastOption option;
            option.title   = "Already processing";
            option.message = "The background task is still running.";
            option.level   = ToastLevel::Warning;
            toast_host->show(std::move(option));
            return;
        }

        if (worker.joinable())
        {
            worker.join();
        }

        cancel_processing = false;
        worker            = std::thread([toast_host, &processing, &cancel_processing]
                             {
                                 const int total = 150;

                                 ToastOption started;
                                 started.title    = "Processing files";
                                 started.message  = "0 / " + std::to_string(total) + " files processed";
                                 started.level    = ToastLevel::Info;
                                 started.progress = 0.0F;
                                 started.timeout  = std::chrono::milliseconds(0);
                                 const ToastId id = toast_host->show(std::move(started));

                                 for (int index = 1; index <= total && !cancel_processing; ++index)
                                 {
                                     std::this_thread::sleep_for(25ms);

                                     ToastOption update;
                                     update.title    = "Processing files";
                                     update.message  = std::to_string(index) + " / " + std::to_string(total) + " files processed";
                                     update.level    = ToastLevel::Info;
                                     update.progress = static_cast<float>(index) / static_cast<float>(total);
                                     update.timeout  = std::chrono::milliseconds(0);
                                     toast_host->update(id, std::move(update));
                                 }

                                 if (!cancel_processing)
                                 {
                                     ToastOption done;
                                     done.title    = "Processing complete";
                                     done.message  = std::to_string(total) + " / " + std::to_string(total) + " files processed";
                                     done.level    = ToastLevel::Success;
                                     done.progress = 1.0F;
                                     done.timeout  = 2s;
                                     toast_host->update(id, std::move(done));
                                 }

                                 processing = false;
                             });
    };

    auto component = toast_host | ftxui::CatchEvent([&](ftxui::Event event)
                                                   {
                                                       if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q'))
                                                       {
                                                           cancel_processing = true;
                                                           screen.ExitLoopClosure()();
                                                           return true;
                                                       }

                                                       if (event == ftxui::Event::Character('i'))
                                                       {
                                                           ToastOption option;
                                                           option.title   = "Status";
                                                           option.message = "Settings saved successfully.";
                                                           option.level   = ToastLevel::Success;
                                                           toast_host->show(std::move(option));
                                                           return true;
                                                       }

                                                       if (event == ftxui::Event::Character('e'))
                                                       {
                                                           ToastOption option;
                                                           option.title   = "Background task failed";
                                                           option.message = "Unable to open the selected file.";
                                                           option.level   = ToastLevel::Error;
                                                           option.timeout = 4s;
                                                           toast_host->show(std::move(option));
                                                           return true;
                                                       }

                                                       if (event == ftxui::Event::Character('p'))
                                                       {
                                                           start_processing();
                                                           return true;
                                                       }

                                                       return false;
                                                   });

    screen.Loop(component);

    cancel_processing = true;
    if (worker.joinable())
    {
        worker.join();
    }

    return 0;
}
