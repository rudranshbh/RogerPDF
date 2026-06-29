#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#define NOMINMAX
#include <shobjidl.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>
#include <MemoryBuffer.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

struct __declspec(uuid("5b0d3235-4dba-4d4d-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

namespace winrt::RogerPDF::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
        fz_register_document_handlers(ctx);
    }

    void MainWindow::UpdatePageUI()
    {
        m_updatingUI = true;
        PageInputBox().Text(winrt::to_hstring(currentPage + 1));
        PageTotalText().Text(L"/ " + winrt::to_hstring(totalPages));
        m_updatingUI = false;
    }

    void MainWindow::UpdateZoomUI()
    {
        m_updatingUI = true;
        int pct = (int)std::round(currentZoom * 100.0f);
        int clamped = pct < 40 ? 40 : (pct > 400 ? 400 : pct);
        ZoomSlider().Value((double)clamped);
        ZoomInputBox().Text(winrt::to_hstring(pct));
        m_updatingUI = false;
    }

    void MainWindow::CommitPageInput()
    {
        if (!doc) return;
        try {
            int page = std::stoi(winrt::to_string(PageInputBox().Text())) - 1;
            if (page < 0) page = 0;
            if (page >= totalPages) page = totalPages - 1;
            currentPage = page;
            UpdatePageUI();
            Render();
        }
        catch (...) { UpdatePageUI(); }
    }

    void MainWindow::CommitZoomInput()
    {
        try {
            int pct = std::stoi(winrt::to_string(ZoomInputBox().Text()));
            if (pct < 40) pct = 40;
            if (pct > 400) pct = 400;
            currentZoom = (float)pct / 100.0f;
            m_viewMode = ViewMode::Normal;
            UpdateZoomUI();
            if (doc) Render();
        }
        catch (...) { UpdateZoomUI(); }
    }

    void MainWindow::Render()
    {
        if (!doc) return;
        if (m_continuousMode) RenderAllPagesAsync();
        else RenderPageAsync();
    }

    // Debounced render: waits 150ms after last zoom gesture before re-rendering.
    // During the wait the ScrollViewer shows the stretched bitmap (fast),
    // then we snap to a crisp re-render once the gesture settles.
    winrt::Windows::Foundation::IAsyncAction MainWindow::RenderDebounced()
    {
        uint64_t myGen = ++m_zoomDebounceGen;

        co_await winrt::resume_after(std::chrono::milliseconds(150));

        if (m_zoomDebounceGen != myGen) co_return;

        // Reset ScrollViewer zoom to 1.0 — suppress the ViewChanged callback
        // so it doesn't try to fold zf=1.0 into currentZoom again
        m_suppressViewChanged = true;
        PdfScrollViewer().ChangeView(nullptr, nullptr, 1.0f);
        m_suppressViewChanged = false;

        Render();
    }

    void MainWindow::OnGridLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        RootGrid().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::OnKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.Key();
        if (key == Windows::System::VirtualKey::Left)
        {
            if (!doc || m_continuousMode || currentPage <= 0) return;
            currentPage--;
            UpdatePageUI();
            Render();
            e.Handled(true);
        }
        else if (key == Windows::System::VirtualKey::Right)
        {
            if (!doc || m_continuousMode || currentPage >= totalPages - 1) return;
            currentPage++;
            UpdatePageUI();
            Render();
            e.Handled(true);
        }
        else if (key == Windows::System::VirtualKey::Up)
        {
            PdfScrollViewer().ChangeView(nullptr, PdfScrollViewer().VerticalOffset() - 120, nullptr);
            e.Handled(true);
        }
        else if (key == Windows::System::VirtualKey::Down)
        {
            PdfScrollViewer().ChangeView(nullptr, PdfScrollViewer().VerticalOffset() + 120, nullptr);
            e.Handled(true);
        }
    }

    void MainWindow::OnPageInputKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            CommitPageInput();
            RootGrid().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
            e.Handled(true);
        }
    }

    void MainWindow::OnPageInputLostFocus(IInspectable const&, RoutedEventArgs const&) { CommitPageInput(); }

    void MainWindow::OnZoomInputKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            CommitZoomInput();
            RootGrid().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
            e.Handled(true);
        }
    }

    void MainWindow::OnZoomInputLostFocus(IInspectable const&, RoutedEventArgs const&) { CommitZoomInput(); }

    // This fires during touchpad pinch AND after our own ChangeView(nullptr,nullptr,1.0f) resets.
    // We use m_suppressViewChanged to ignore our own resets.
    void MainWindow::OnScrollViewerViewChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& e)
    {
        if (m_updatingUI) return;
        if (m_suppressViewChanged) return;

        float zf = PdfScrollViewer().ZoomFactor();
        if (std::abs(zf - 1.0f) < 0.005f) return; // nothing meaningful changed

        // Fold pinch zoom into currentZoom
        float newZoom = currentZoom * zf;
        if (newZoom < 0.4f) newZoom = 0.4f;
        if (newZoom > 4.0f) newZoom = 4.0f;
        currentZoom = newZoom;
        m_viewMode = ViewMode::Normal;
        UpdateZoomUI();

        if (doc) RenderDebounced();
    }

    void MainWindow::OnScrollViewerSizeChanged(IInspectable const&, Microsoft::UI::Xaml::SizeChangedEventArgs const&)
    {
        if (!doc) return;
        if (m_viewMode == ViewMode::FitWidth) ApplyFitWidth();
        else if (m_viewMode == ViewMode::FitPage) ApplyFitPage();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::OnAboutClick(IInspectable const&, RoutedEventArgs const&)
    {
        Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(winrt::box_value(L"About Roger PDF"));
        dialog.CloseButtonText(L"Close");

        Microsoft::UI::Xaml::Controls::ScrollViewer sv;
        sv.MaxHeight(400);

        Microsoft::UI::Xaml::Controls::StackPanel panel;
        panel.Spacing(12);
        panel.Padding(Microsoft::UI::Xaml::Thickness{ 0, 4, 0, 4 });

        auto addText = [&](winrt::hstring text, bool bold = false, double size = 14.0) {
            Microsoft::UI::Xaml::Controls::TextBlock tb;
            tb.Text(text);
            tb.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
            tb.FontSize(size);
            if (bold) tb.FontWeight({ 700 });
            panel.Children().Append(tb);
            };

        addText(L"Roger PDF v1.0", true, 20.0);
        addText(L"Developed by Rudransh Bhardwaj", false, 13.0);
        addText(L"Overview", true, 15.0);
        addText(L"Roger PDF is a high-performance, native Windows PDF viewer built on the MuPDF rendering engine. "
            L"Designed for speed, clarity, and a seamless Windows 11 experience.", false, 13.0);
        addText(L"Technical Stack", true, 15.0);
        addText(L"\u2022 Language: C++20 with WinRT projections\n"
            L"\u2022 UI Framework: WinUI 3 (Windows App SDK)\n"
            L"\u2022 Rendering Engine: MuPDF / libfitz\n"
            L"\u2022 System Backdrop: Mica (BaseAlt)", false, 13.0);
        addText(L"License", true, 15.0);
        addText(L"MuPDF is used under the GNU Affero General Public License v3 (AGPL). "
            L"Roger PDF is a personal project and is not affiliated with Artifex Software.", false, 13.0);

        sv.Content(panel);
        dialog.Content(sv);
        co_await dialog.ShowAsync();
    }

    Windows::Foundation::IAsyncAction MainWindow::OpenButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Windows::Storage::Pickers::FileOpenPicker picker;
        HWND hwnd{ nullptr };
        this->try_as<IWindowNative>()->get_WindowHandle(&hwnd);
        picker.as<IInitializeWithWindow>()->Initialize(hwnd);
        picker.ViewMode(Windows::Storage::Pickers::PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L".pdf");

        auto file = co_await picker.PickSingleFileAsync();
        if (!file) co_return;

        if (doc) { fz_drop_document(ctx, doc); doc = nullptr; }
        std::string path = winrt::to_string(file.Path());

        fz_try(ctx) {
            doc = fz_open_document(ctx, path.c_str());
        } fz_catch(ctx) {
            PageInputBox().Text(L"Error");
            co_return;
        }

        totalPages = fz_count_pages(ctx, doc);
        currentPage = 0;

        m_continuousMode = false;
        m_viewMode = ViewMode::Normal;
        BtnContinuous().IsChecked(false);
        PdfDisplayImage().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        ContinuousPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        ContinuousPanel().Children().Clear();
        BtnPrev().IsEnabled(true);
        BtnNext().IsEnabled(true);

        UpdatePageUI();
        UpdateZoomUI();
        m_suppressViewChanged = true;
        PdfScrollViewer().ChangeView(0.0, 0.0, 1.0f);
        m_suppressViewChanged = false;

        RootGrid().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
        co_await RenderPageAsync();
    }

    Windows::Foundation::IAsyncAction MainWindow::RenderPageAsync()
    {
        if (!doc) co_return;

        fz_page* page = nullptr;
        fz_pixmap* pix = nullptr;
        fz_device* dev = nullptr;

        fz_try(ctx) {
            float dpr = (float)this->Content().XamlRoot().RasterizationScale();
            fz_matrix matrix = fz_scale(currentZoom * dpr, currentZoom * dpr);
            page = fz_load_page(ctx, doc, currentPage);
            fz_rect rect = fz_bound_page(ctx, page);
            fz_irect bbox = fz_round_rect(fz_transform_rect(rect, matrix));
            pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 0);
            fz_clear_pixmap_with_value(ctx, pix, 255);
            dev = fz_new_draw_device(ctx, matrix, pix);
            fz_run_page(ctx, page, dev, fz_identity, nullptr);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
            dev = nullptr;
        } fz_catch(ctx) {
            if (dev) fz_drop_device(ctx, dev);
            if (pix) fz_drop_pixmap(ctx, pix);
            if (page) fz_drop_page(ctx, page);
            co_return;
        }

        int w = pix->w, h = pix->h;
        std::vector<uint8_t> bgraData(w * h * 4);
        unsigned char* src = pix->samples;
        for (int i = 0; i < w * h; ++i) {
            bgraData[i * 4 + 0] = src[i * 3 + 2];
            bgraData[i * 4 + 1] = src[i * 3 + 1];
            bgraData[i * 4 + 2] = src[i * 3 + 0];
            bgraData[i * 4 + 3] = 255;
        }
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);

        try {
            Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap wb(w, h);
            memcpy(wb.PixelBuffer().data(), bgraData.data(), bgraData.size());
            wb.Invalidate();
            PdfDisplayImage().Source(wb);
        }
        catch (...) {}
    }

    void MainWindow::Prev_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentPage <= 0) return;
        currentPage--;
        UpdatePageUI();
        Render();
    }

    void MainWindow::Next_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentPage >= totalPages - 1) return;
        currentPage++;
        UpdatePageUI();
        Render();
    }

    void MainWindow::ZoomIn_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc) return;
        currentZoom += 0.25f;
        if (currentZoom > 4.0f) currentZoom = 4.0f;
        m_viewMode = ViewMode::Normal; // user took manual control, stop reapplying fit on resize
        UpdateZoomUI();
        Render();
    }

    void MainWindow::ZoomOut_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentZoom <= 0.4f) return;
        currentZoom -= 0.25f;
        if (currentZoom < 0.4f) currentZoom = 0.4f;
        m_viewMode = ViewMode::Normal; // user took manual control, stop reapplying fit on resize
        UpdateZoomUI();
        Render();
    }

    void MainWindow::ZoomSlider_ValueChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        if (m_updatingUI) return;
        if (!ZoomInputBox()) return;
        if (!ZoomSlider()) return;
        currentZoom = (float)e.NewValue() / 100.0f;
        m_viewMode = ViewMode::Normal;
        m_updatingUI = true;
        ZoomInputBox().Text(winrt::to_hstring((int)std::round(e.NewValue())));
        m_updatingUI = false;
        // Slider gets a debounced render too — dragging fast won't hammer MuPDF
        if (doc) RenderDebounced();
    }

    void MainWindow::ApplyFitWidth()
    {
        if (!doc) return;
        float viewportWidth = (float)PdfScrollViewer().ActualWidth() - 80.0f;
        if (viewportWidth <= 0) return;
        float dpr = (float)this->Content().XamlRoot().RasterizationScale();
        fz_page* page = fz_load_page(ctx, doc, currentPage);
        fz_rect rect = fz_bound_page(ctx, page);
        fz_drop_page(ctx, page);
        float pageWidth = rect.x1 - rect.x0;
        // Rendered pixel width = pageWidth * currentZoom * dpr
        // We want that == viewportWidth, so currentZoom = viewportWidth / (pageWidth * dpr)
        if (pageWidth > 0) currentZoom = viewportWidth / (pageWidth * dpr);
        UpdateZoomUI();
        Render();
    }

    void MainWindow::FitWidth_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_viewMode = ViewMode::FitWidth;
        ApplyFitWidth();
    }

    void MainWindow::ApplyFitPage()
    {
        if (!doc) return;
        float viewportW = (float)PdfScrollViewer().ActualWidth() - 80.0f;
        float viewportH = (float)PdfScrollViewer().ActualHeight() - 80.0f;
        if (viewportW <= 0 || viewportH <= 0) return;
        float dpr = (float)this->Content().XamlRoot().RasterizationScale();
        fz_page* page = fz_load_page(ctx, doc, currentPage);
        fz_rect rect = fz_bound_page(ctx, page);
        fz_drop_page(ctx, page);
        float pageW = rect.x1 - rect.x0;
        float pageH = rect.y1 - rect.y0;
        float zoomW = (pageW > 0) ? viewportW / (pageW * dpr) : 1.0f;
        float zoomH = (pageH > 0) ? viewportH / (pageH * dpr) : 1.0f;
        currentZoom = (zoomW < zoomH) ? zoomW : zoomH;
        UpdateZoomUI();
        Render();
    }

    void MainWindow::FitPage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_viewMode = ViewMode::FitPage;
        ApplyFitPage();
    }

    void MainWindow::Continuous_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_continuousMode = BtnContinuous().IsChecked().GetBoolean();
        if (m_continuousMode)
        {
            PdfDisplayImage().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            ContinuousPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            BtnPrev().IsEnabled(false);
            BtnNext().IsEnabled(false);
            if (doc) RenderAllPagesAsync();
        }
        else
        {
            m_renderGeneration++; // cancel any in-flight continuous render
            PdfDisplayImage().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            ContinuousPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            ContinuousPanel().Children().Clear();
            BtnPrev().IsEnabled(true);
            BtnNext().IsEnabled(true);
            if (doc) RenderPageAsync();
        }
    }

    Windows::Foundation::IAsyncAction MainWindow::RenderAllPagesAsync()
    {
        if (!doc) co_return;

        uint64_t myGeneration = ++m_renderGeneration;
        ContinuousPanel().Children().Clear();

        float dpr = (float)this->Content().XamlRoot().RasterizationScale();

        for (int i = 0; i < totalPages; ++i)
        {
            if (m_renderGeneration != myGeneration) co_return;

            fz_page* page = nullptr;
            fz_pixmap* pix = nullptr;
            fz_device* dev = nullptr;

            fz_try(ctx) {
                fz_matrix matrix = fz_scale(currentZoom * dpr, currentZoom * dpr);
                page = fz_load_page(ctx, doc, i);
                fz_rect rect = fz_bound_page(ctx, page);
                fz_irect bbox = fz_round_rect(fz_transform_rect(rect, matrix));
                pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 0);
                fz_clear_pixmap_with_value(ctx, pix, 255);
                dev = fz_new_draw_device(ctx, matrix, pix);
                fz_run_page(ctx, page, dev, fz_identity, nullptr);
                fz_close_device(ctx, dev);
                fz_drop_device(ctx, dev); dev = nullptr;
            } fz_catch(ctx) {
                if (dev) fz_drop_device(ctx, dev);
                if (pix) fz_drop_pixmap(ctx, pix);
                if (page) fz_drop_page(ctx, page);
                continue;
            }

            int w = pix->w, h = pix->h;
            std::vector<uint8_t> bgraData(w * h * 4);
            unsigned char* src = pix->samples;
            for (int j = 0; j < w * h; ++j) {
                bgraData[j * 4 + 0] = src[j * 3 + 2];
                bgraData[j * 4 + 1] = src[j * 3 + 1];
                bgraData[j * 4 + 2] = src[j * 3 + 0];
                bgraData[j * 4 + 3] = 255;
            }
            fz_drop_pixmap(ctx, pix);
            fz_drop_page(ctx, page);

            if (m_renderGeneration != myGeneration) co_return;

            try {
                Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap wb(w, h);
                memcpy(wb.PixelBuffer().data(), bgraData.data(), bgraData.size());
                wb.Invalidate();

                Microsoft::UI::Xaml::Controls::Image img;
                img.Source(wb);
                img.Stretch(Microsoft::UI::Xaml::Media::Stretch::None);
                img.HorizontalAlignment(Microsoft::UI::Xaml::HorizontalAlignment::Center);
                Microsoft::UI::Xaml::Media::ThemeShadow shadow;
                img.Shadow(shadow);

                ContinuousPanel().Children().Append(img);
            }
            catch (...) {}
        }
    }
}