#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <shobjidl.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.System.h>
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
        ZoomSlider().Value((double)pct);
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
            RenderPageAsync();
        }
        catch (...) {
            UpdatePageUI(); // reset to valid value
        }
    }

    void MainWindow::CommitZoomInput()
    {
        try {
            int pct = std::stoi(winrt::to_string(ZoomInputBox().Text()));
            if (pct < 40) pct = 40;
            if (pct > 400) pct = 400;
            currentZoom = (float)pct / 100.0f;
            UpdateZoomUI();
            if (doc) RenderPageAsync();
        }
        catch (...) {
            UpdateZoomUI(); // reset to valid value
        }
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
            if (!doc || currentPage <= 0) return;
            currentPage--;
            UpdatePageUI();
            RenderPageAsync();
            e.Handled(true);
        }
        else if (key == Windows::System::VirtualKey::Right)
        {
            if (!doc || currentPage >= totalPages - 1) return;
            currentPage++;
            UpdatePageUI();
            RenderPageAsync();
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

    void MainWindow::OnPageInputLostFocus(IInspectable const&, RoutedEventArgs const&)
    {
        CommitPageInput();
    }

    void MainWindow::OnZoomInputKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            CommitZoomInput();
            RootGrid().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
            e.Handled(true);
        }
    }

    void MainWindow::OnZoomInputLostFocus(IInspectable const&, RoutedEventArgs const&)
    {
        CommitZoomInput();
    }

    void MainWindow::OnScrollViewerViewChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const&)
    {
        if (m_updatingUI) return;
        float zoomFactor = PdfScrollViewer().ZoomFactor();
        m_updatingUI = true;
        int pct = (int)std::round(zoomFactor * currentZoom * 100.0f);
        ZoomSlider().Value((double)std::clamp(pct, 40, 400));
        ZoomInputBox().Text(winrt::to_hstring(std::clamp(pct, 40, 400)));
        m_updatingUI = false;
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
            L"Designed for speed, clarity, and a seamless Windows 11 experience, it delivers pixel-accurate "
            L"document rendering with a lightweight footprint — no Electron, no web views, no compromises.", false, 13.0);

        addText(L"Technical Stack", true, 15.0);
        addText(L"• Language: C++20 with WinRT projections\n"
            L"• UI Framework: WinUI 3 (Windows App SDK)\n"
            L"• Rendering Engine: MuPDF / libfitz\n"
            L"• Build System: MSBuild / Visual Studio 2025\n"
            L"• System Backdrop: Mica (BaseAlt) for native Windows 11 aesthetics", false, 13.0);

        addText(L"Core Features", true, 15.0);
        addText(L"• Hardware-accelerated PDF rendering via MuPDF's fitz engine\n"
            L"• Smooth page navigation with Previous / Next controls and direct page input\n"
            L"• Precision zoom control via slider, direct percentage input, and touchpad pinch gesture\n"
            L"• Keyboard navigation: arrow keys for scrolling and page switching\n"
            L"• High-DPI aware rendering with per-monitor DPI scaling\n"
            L"• Native Windows 11 design with Mica backdrop, Fluent icons, and shadow effects\n"
            L"• Extends content into title bar for a modern, immersive layout\n"
            L"• Zero external runtime dependencies — ships as a single native executable", false, 13.0);

        addText(L"Performance", true, 15.0);
        addText(L"Roger PDF renders directly to a WriteableBitmap via raw pixel buffer access, "
            L"bypassing managed imaging pipelines entirely. This ensures sub-frame render times "
            L"for typical document pages, with full support for complex vector graphics, embedded fonts, "
            L"and mixed raster/vector content.", false, 13.0);

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
        UpdatePageUI();
        UpdateZoomUI();
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

        int w = pix->w;
        int h = pix->h;
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
            Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap writeableBitmap(w, h);
            uint8_t* dataIn = writeableBitmap.PixelBuffer().data();
            memcpy(dataIn, bgraData.data(), bgraData.size());
            writeableBitmap.Invalidate();
            PdfDisplayImage().Source(writeableBitmap);
        }
        catch (...) {}
    }

    void MainWindow::Prev_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentPage <= 0) return;
        currentPage--;
        UpdatePageUI();
        RenderPageAsync();
    }

    void MainWindow::Next_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentPage >= totalPages - 1) return;
        currentPage++;
        UpdatePageUI();
        RenderPageAsync();
    }

    void MainWindow::ZoomIn_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc) return;
        currentZoom += 0.2f;
        UpdateZoomUI();
        RenderPageAsync();
    }

    void MainWindow::ZoomOut_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc || currentZoom <= 0.4f) return;
        currentZoom -= 0.2f;
        UpdateZoomUI();
        RenderPageAsync();
    }

    void MainWindow::ZoomSlider_ValueChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        if (m_updatingUI) return;
        if (!ZoomInputBox()) return;
        if (!ZoomSlider()) return;
        currentZoom = (float)e.NewValue() / 100.0f;
        m_updatingUI = true;
        ZoomInputBox().Text(winrt::to_hstring((int)std::round(e.NewValue())));
        m_updatingUI = false;
        if (doc) RenderPageAsync();
    }


}