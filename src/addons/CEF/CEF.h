#ifndef CEF_H_INCLUDED
#define CEF_H_INCLUDED

#include <OpenSG/OSGConfig.h>

#include "core/utils/VRFunctionFwd.h"
#include "core/objects/VRObjectFwd.h"

#ifdef PLOG
#undef PLOG
#endif

#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_load_handler.h"


using namespace std;
OSG_BEGIN_NAMESPACE;

class VRDevice;

class CEF_handler : public CefRenderHandler, public CefLoadHandler, public CefContextMenuHandler, public CefDialogHandler, public CefDisplayHandler {
    private:
        VRTexturePtr image = 0;
        int width = 1024;
        int height = 1024;

    public:
        CEF_handler();
        ~CEF_handler();

#ifdef _WIN32
        void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
#else
        bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
#endif

        void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model);
        bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags event_flags);

        void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;
        VRTexturePtr getImage();
        void resize(int resolution, float aspect);

        void OnLoadEnd( CefRefPtr< CefBrowser > browser, CefRefPtr< CefFrame > frame, int httpStatusCode ) override;
        void OnLoadError( CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl ) override;
        void OnLoadStart( CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type ) override;

        bool OnFileDialog( CefRefPtr< CefBrowser > browser, CefDialogHandler::FileDialogMode mode, const CefString& title, const CefString& default_file_path, const std::vector< CefString >& accept_filters, int selected_accept_filter, CefRefPtr< CefFileDialogCallback > callback ) override;

        void on_link_clicked(string source, int line, string s);
        bool OnConsoleMessage( CefRefPtr< CefBrowser > browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line ) override;

        IMPLEMENT_REFCOUNTING(CEF_handler);
};

class CEF_client : public CefClient {
    private:
        CefRefPtr<CEF_handler> handler;

    public:
        CEF_client();
        ~CEF_client();

        CefRefPtr<CEF_handler> getHandler();
        CefRefPtr<CefRenderHandler> GetRenderHandler() override;
        CefRefPtr<CefLoadHandler> GetLoadHandler() override;
        CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override;
        CefRefPtr<CefDialogHandler> GetDialogHandler() override;
        CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;

        IMPLEMENT_REFCOUNTING(CEF_client);
};

class CEF {
    private:
        CefRefPtr<CefBrowser> browser;
        CefRefPtr<CEF_client> client;

        string site;
        VRMaterialWeakPtr mat;
        VRObjectWeakPtr obj;
        int resolution = 1024;
        float aspect = 1;
        bool init = false;
        bool focus = false;
        bool ctrlUsed = false;
        bool doMouse = true;
        bool doKeyboard = true;
        int mX = -1;
        int mY = -1;

        VRUpdateCbPtr update_callback;
        map<VRDevice*, VRDeviceCbPtr> mouse_dev_callback;
        map<VRDevice*, VRUpdateCbPtr> mouse_move_callback;
        //VRDeviceCbPtr mouse_dev_callback;
        VRDeviceCbPtr keyboard_dev_callback;

        void global_initiate();
        void initiate();
        void update();

        bool mouse(int lb, int rb, int wu, int wd, VRDeviceWeakPtr dev);
        void mouse_move(VRDeviceWeakPtr dev);
        bool keyboard(VRDeviceWeakPtr dev);

        CEF();
    public:
        ~CEF();
        static shared_ptr<CEF> create();

        void setResolution(float a);
        void setAspectRatio(float a);

        void setMaterial(VRMaterialPtr mat);
        void addMouse(VRDevicePtr dev, VRObjectPtr obj, int lb, int rb, int wu, int wd);
        void addKeyboard(VRDevicePtr dev);
        void toggleInput(bool keyboard, bool mouse);

        void open(string site);
        void reload();
        string getSite();
        void resize();

        static vector< shared_ptr<CEF> > getInstances();
        static void reloadScripts(string path);
        static void shutdown();
};

typedef shared_ptr<CEF> CEFPtr;

OSG_END_NAMESPACE;

#endif // CAVEKEEPER_H_INCLUDED
