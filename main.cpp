
#define TGAP 10
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <opencv2/opencv.hpp>
#include<vector>

#include <X11/Xutil.h>
#include<stdlib.h>
#include<X11/Xlib.h>
#include<X11/cursorfont.h>

#include <sys/un.h>

#include <getopt.h>

#include <fontconfig/fontconfig.h> 

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xft/Xft.h>

#include "translate.hpp"
#include "align.hpp"

using namespace cv;
using namespace std;

#define PATH_TO_TESSDATA NULL // set "path/to/your/tessdata" or use NULL for autodetection
#define FONTNAME "monospace"   
const char* DEFAULT_LANGUAGE = "eng+rus";
const char* TRANSLATION_LANGUAGE = "ru";

XRenderColor xrcolor = {0xffff, 0xffff, 0xffff, 0xffff};     // color of text
XRenderColor xrcolor_stroke = {0, 0, 0, 0xffff};     // color of stroke
XRenderColor darkBG = {0, 0, 0, 0xffff};

#define BUFFERSIZE 8192 // block size while sending to daemon
const char* bufsocket = "/tmp/bufcvnip.sock";

static int wiwidth;
static int wiheight;

int TRANSLATED = 0;


class Word{
    public:
        int x2, y2;
    
    string translation;
    int x1, y1, w, h;
    string buff;
    string text;
    Word(string text_, int x_, int y_, int x2_, int y2_){
        x1 = std::min(x_, x2_);
        x2 = std::max(x_, x2_);
        y1 = std::min(y_, y2_);
        y2 = std::max(y_, y2_);
        w = x_ - x2_; h = y_ - y2_;
        text = text_;

    }
    void add_translation(string n){
        if (translation.size() > 0) translation += " ";
        translation += n;
    }
    void set_translation(){
        if (translation.size()==0){translation=" ";}
        buff = text;
        text = translation;
    }

    void SetText(string nt){text=nt;}


};

class Block{
    public:
    int x, y, x2, y2;
    vector<Word>words;
    Word trashword;

    Block(int xv, int yv, int x22, int y22, vector<Word>&w):x(xv),y(yv),x2(x22),y2(y22), trashword("err", -100, -100, 0, 0) {
        words.swap(w);
    }
    void Translate(vector<Word>newwords){
        return;
    }
    Word& operator[](ssize_t i){return words[i];}

    Word& FindByName(string name){
        for(int i=0; i<words.size();i++){
            if (name == words[i].text){
                return words[i];
            }
        }
        return trashword;
    }

};

vector<Block>blocks;

class Button{
    public:

    string name; string text;
    XftColor color;
    int x, y, w, h;
    XftFont *font;
    int kostil=0;
    bool havefont=0;
    Button(string n, string t, int x1, int y1, int w1, int h1, void(*func)(Display* dpy, Window window)){
        x=x1;y=y1;w=w1;h=h1;name=n;text=t; Call = func;

    }
    void SetColor(XftColor col){
        color = col;
    }
    void (*Call)(Display* disp, Window window);
    void Draw(Display* disp, Window window, GC gc, int screen, XftDraw* draw){
        if (!havefont){
            font = XftFontOpenName(disp, DefaultScreen(disp), (string(FONTNAME) + "-16").c_str());
            havefont=1;
        }
        XSetForeground(disp, gc, 0xFFFFFFFF);
        XFillRectangle(disp, window, gc, x, y, w, h);
        XSetForeground(disp, gc, BlackPixel(disp, screen));
        XDrawRectangle(disp, window, gc, x, y, w, h);

        XftDrawStringUtf8(draw, &color, font, kostil + x+10, y+20, (FcChar8*)text.c_str(), text.size());
    }
    void SetPos(int x1, int y1){x=x1;y=y1;}
    void SetKostilValue(int kk){
        kostil = kk; // very useful var btw
    }


};

int RESULT_SIZE = -1;

XftFont* loadFontFitting(Display* disp, int screen, const char* family,
                         int maxSize, int targetWidth, int targetHeight,
                         const char* text) {
    XGlyphInfo extents;
    XftFont* chosen = nullptr;
    int chosenSize = 0;

    // Ищем подходящий по ширине
    for (int size = maxSize; size >= 6; size--) {
        std::string fontName = std::string(family) + "-" + std::to_string(size);
        XftFont* font = XftFontOpenName(disp, screen, fontName.c_str());
        if (!font) continue;

        XftTextExtentsUtf8(disp, font, (FcChar8*)text, strlen(text), &extents);

        if (extents.width <= targetWidth) {
            chosen = font;
            chosenSize = size;
            break;
        }
        XftFontClose(disp, font);
    }

    // Если ничего не нашли — fallback на минимальный
    if (!chosen) {
        chosenSize = 6;
        std::string fontName = std::string(family) + "-" + std::to_string(chosenSize);
        chosen = XftFontOpenName(disp, screen, fontName.c_str());
        if (!chosen) return nullptr;
        XftTextExtentsUtf8(disp, chosen, (FcChar8*)text, strlen(text), &extents);
    }

    // Проверяем высоту
    int naturalHeight = chosen->ascent + chosen->descent;
    if (naturalHeight > targetHeight) {
        double scaleY = (double)targetHeight / (double)naturalHeight;

        FcMatrix m;
        FcMatrixInit(&m);
        FcMatrixScale(&m, 1.0, scaleY); // сжать по Y

        XftPattern* pat = XftPatternCreate();
        XftPatternAddString(pat, XFT_FAMILY, family);
        XftPatternAddDouble(pat, XFT_SIZE, (double)chosenSize);
        XftPatternAddMatrix(pat, XFT_MATRIX, &m);

        XftFont* scaled = XftFontOpenPattern(disp, pat);
        if (scaled) {
            XftFontClose(disp, chosen);
            chosen = scaled;
        }
    }

    RESULT_SIZE = chosenSize;
    return chosen;
}


Mat getmeth(XImage* s){
    int d = s->bits_per_pixel;
    int ch = d / 8;
    Mat mat(s->height, s->width, CV_MAKETYPE(CV_8U, ch), s->data);
    return mat.clone();
}

void Copy(Display* disp, Window window){
    string text;

    if (blocks.size()){
    for(int i=0; i<blocks.size(); i++){
        std::cout<<i<<"\n";
        if (i>0){if (blocks[i][0].y1 - blocks[i-1][blocks[i-1].words.size()-1].y1 > 10){text += "\n";} 
        else{text += " ";}}
    for(int j=0; j<blocks[i].words.size()-1;j++){
        text += blocks[i].words[j].text;
        // if next text if 10px under previous one, the \n is inserted to the beginning of the new line
        if (blocks[i].words[j+1].y1 - blocks[i].words[j].y1 > 10) text += "\n";
        else text += " ";
    }
    text += blocks[i][blocks[i].words.size()-1].text;
    
    }}
    std::cout<<"sending to "<<bufsocket<<"\n";
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0){ std::cout<<"error in socket creation\n"; close(fd); return;}
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, bufsocket, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return;
    }
    write(fd, text.c_str(), text.size());
    close(fd);
    return;
}

void Exit(Display* disp, Window window){
    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(disp, "MY_CUSTOM_MESSAGE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 42;
    event.xclient.data.l[1] = 0;  

    XSendEvent(disp, window, False, NoEventMask, &event);
}

void Translate_start(Display* disp, Window window){

    if (blocks.empty()) return;

    for(size_t i=0; i<blocks.size(); i++){

        if (blocks[i].words.empty()) continue;

        string text;
        for(size_t j=0; j<blocks[i].words.size(); ++j){
            text += blocks[i].words[j].text;

            //blocks[i].words[j].translation = "";

            if (j+1 < blocks[i].words.size()){
                if (blocks[i].words[j+1].y1 - blocks[i].words[j].y1 > 10)
                    text += " ";
                else
                    text += " ";
            }
        }

        string res = Translate(text, "auto", TRANSLATION_LANGUAGE);

        fast_align model;
        model.loadmodel();

        vector<string> src = Split(text, ' ');
        vector<string> tgt = Split(res, ' ');

        if (!src.empty() && !tgt.empty()){
            model.train_pair(src, tgt, 50);
            model.savemodel();

            auto alig = model.align(src, tgt);

            for (auto [src_idx, tgt_idx] : alig){
                if (src_idx >= src.size() || tgt_idx >= tgt.size()) continue;

                string src_word = src[src_idx];
                string tgt_word = tgt[tgt_idx];

                Word& w = blocks[i].FindByName(src_word);

                if (w.text != "err")
                std::cout<<src_word<<" "<<tgt_word<<"\n";
                    w.add_translation(tgt_word);
            }
        }

        for (auto& w : blocks[i].words){
            w.set_translation();
        }
    }
    TRANSLATED = 1;
}

int main(int argc, char** argv) {

    int opt;
    while ((opt = getopt(argc, argv, "d:t:h")) != -1){
        switch(opt){
            case 'd':
                DEFAULT_LANGUAGE = optarg;
                break;
            case 'h':
                std::cout<<"cvnip librejokes.ru. -d <text> for setting default language\n Libre Translator requires en and ru, whereas OCR requires\n eng and rus (or eng+rus)\n";
                return 0;
            case 't':
                TRANSLATION_LANGUAGE = optarg;
                break;
            default:
                std::cout<<"nu such option\n";
                return 0;
        }
    }

  static int VisData[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 16,
        None
        };
    int rx = 0, ry = 0, rw = 0, rh = 0;
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    int btn_pressed = 0, done = 0;

    XEvent ev;
    Display *disp = XOpenDisplay(NULL);

    if(!disp)
        return EXIT_FAILURE;

    Screen *scr = NULL;
    scr = ScreenOfDisplay(disp, DefaultScreen(disp));

    Window root = 0, window;

    root = RootWindow(disp, XScreenNumberOfScreen(scr));

    Cursor cursor, cursor2;
    cursor = XCreateFontCursor(disp, XC_left_ptr);
    cursor2 = XCreateFontCursor(disp, XC_lr_angle);

    static XVisualInfo *visual;
    static XRenderPictFormat *pict_format;

    XGCValues gcval;
    static GLXFBConfig *fbconfigs, fbconfig;
    XSetWindowAttributes attr;
    gcval.foreground = XWhitePixel(disp, 0);
    gcval.function = GXxor;
    gcval.background = XBlackPixel(disp, 0);
    gcval.plane_mask = gcval.background ^ gcval.foreground;
    gcval.subwindow_mode = IncludeInferiors;
    int numfbconfigs;

    fbconfigs = glXChooseFBConfig(disp, DefaultScreen(disp), VisData, &numfbconfigs);
    fbconfig = 0;
    for(int i = 0; i<numfbconfigs; i++) {
        visual = (XVisualInfo*) glXGetVisualFromFBConfig(disp, fbconfigs[i]);
        if(!visual)
            continue;

        pict_format = XRenderFindVisualFormat(disp, visual->visual);
        if(!pict_format)
            continue;

        fbconfig = fbconfigs[i];
        if(pict_format->direct.alphaMask > 0) {
            break;
        }
        }

    static Colormap cmap;


    cmap = XCreateColormap(disp, root, visual->visual, AllocNone);

    attr.override_redirect = True;
    attr.colormap = cmap;
    attr.background_pixmap = None;
    attr.border_pixel = 0;

    int attr_mask =
        CWBackPixmap|
        CWColormap|
        CWBorderPixel|
        CWEventMask;

    
    wiwidth = DisplayWidth(disp, DefaultScreen(disp));
    wiheight = DisplayHeight(disp, DefaultScreen(disp));

    window = XCreateWindow(disp, root, 0, 0, wiwidth, wiheight, 0,
        visual->depth, InputOutput,
        visual->visual,
        CWOverrideRedirect | CWColormap | CWBackPixmap | CWBorderPixel, &attr);


        Atom op_at = XInternAtom(disp, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long op_val = 0xFFFFFFFF;
        XChangeProperty(disp, window, op_at, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&op_val, 1);


    GC gc;
    gc = XCreateGC(disp, window,
                    GCFunction | GCForeground | GCBackground | GCSubwindowMode,
                    &gcval);
                    

        XserverRegion region = XFixesCreateRegion(disp, NULL, 0);
        XFixesSetWindowShapeRegion(disp, window, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion(disp, window, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(disp, region);

    XWindowAttributes attrs;
    XGetWindowAttributes(disp, window, &attrs);
        
    XftDraw *draw = XftDrawCreate(disp, window,
                              attrs.visual, attrs.colormap);

    
    XftColor color, color_stroke;
    XftColorAllocValue(disp, attrs.visual, attrs.colormap,
                    &xrcolor, &color);
    XftColorAllocValue(disp, attrs.visual, attrs.colormap,
                    &xrcolor_stroke, &color_stroke);

    XftColor color_darkBG;
    XftColorAllocValue(disp, attrs.visual, attrs.colormap,
                    &darkBG, &color_darkBG);
        

    XftFont *font = XftFontOpenName(disp, DefaultScreen(disp), FONTNAME);



    Atom WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(disp, window, &WM_DELETE_WINDOW, 1);

    vector<Button> btns;
    btns.push_back(
        Button(
            "translate", "translate", 0, 0, 400, 100, Translate_start
        )
    );
    btns.push_back(
        Button(
            "copy", "copy", 0, 0, 400, 100, Copy
        )
    );
    btns.push_back(
        Button(
            "exit", "exit", 500, 0, 400, 100, Exit
        )
    );
    for(int i=0; i<btns.size();i++){btns[i].SetColor(color_darkBG);}
    btns[1].SetKostilValue(150);
    btns[2].SetKostilValue(150);


    XMapWindow(disp, window);
    XRaiseWindow(disp, window);

    if ((XGrabPointer
        (disp, root, False,
            ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
        printf("couldn't grab pointer:");

    if ((XGrabKeyboard
        (disp, root, False, GrabModeAsync, GrabModeAsync,
            CurrentTime) != GrabSuccess))
        printf("couldn't grab keyboard:");


    int ready = 0;

    while ((!done)) {
        if(ready){

        }
        //~ while (!done && XPending(disp)) {
        //~ XNextEvent(disp, &ev);
        else if (!XPending(disp)) { usleep(1000); continue; } // fixes the 100% CPU hog issue in original code
        if ( (XNextEvent(disp, &ev) >= 0) ) {
        switch (ev.type) {
            case KeyPress:
                if (XLookupKeysym(&ev.xkey, 0) == XK_Escape) done = 1;
                break;
            case ClientMessage:
                   done=1;
                   XftDrawDestroy(draw);
                   XftFontClose(disp, font);
                   XCloseDisplay(disp);
                   return EXIT_SUCCESS;
            case MotionNotify:

            if (rect_w) {
                XDrawRectangle(disp, window, gc, rect_x, rect_y, rect_w, rect_h);

                XFlush(disp);
            }
            if (btn_pressed) {
                if (!ready){
                rect_x = rx;
                rect_y = ry;
                rect_w = ev.xmotion.x - rect_x;
                rect_h = ev.xmotion.y - rect_y;

                if (rect_w < 0) {
                rect_x += rect_w;
                rect_w = 0 - rect_w;
                }
                if (rect_h < 0) {
                rect_y += rect_h;
                rect_h = 0 - rect_h;
                }
                /* draw rectangle */
            }
                else{
                    
                }
            }
            else {
                XChangeActivePointerGrab(disp,
                                        ButtonMotionMask | ButtonReleaseMask,
                                        cursor2, CurrentTime);
                }
            break;
            case ButtonPress:
            btn_pressed = 1;
            rx = ev.xbutton.x;
            ry = ev.xbutton.y;
            break;

            case ButtonRelease:
                    if (!ready){
                    rw = ev.xbutton.x - rx;
                    rh = ev.xbutton.y - ry;
                    /* cursor moves backwards */
                    if (rw < 0) {
                        rx += rw;
                        rw = 0 - rw;
                    }
                    if (rh < 0) {
                        ry += rh;
                        rh = 0 - rh;
                    }

                    Mat img = getmeth(XGetImage(disp,root, rx,ry , rw,rh,AllPlanes, ZPixmap));
                    if (img.empty()) return -1;

                    tesseract::TessBaseAPI ocr;
                    if (ocr.Init(PATH_TO_TESSDATA, DEFAULT_LANGUAGE)) { 
                        cerr << "Could not initialize tesseract.\n";
                        return -1;
                    }

                ocr.SetImage(img.data, img.cols, img.rows, img.channels(), img.step);
                //ocr.SetPageSegMode(tesseract::PSM_AUTO);
                ocr.Recognize(0);

                tesseract::ResultIterator* ri = ocr.GetIterator();
                tesseract::PageIteratorLevel block_level = tesseract::RIL_BLOCK;
                tesseract::PageIteratorLevel word_level = tesseract::RIL_WORD;

                if (ri != nullptr) {
                    vector<Word>words;
                    do {
                        
                        float conf = ri->Confidence(block_level);

                        int bx1, by1, bx2, by2;
                        ri->BoundingBox(block_level, &bx1, &by1, &bx2, &by2);
                        words.clear();
                
                        tesseract::ResultIterator* word_i = ri;
                        do{
                        string word = string(word_i->GetUTF8Text(word_level));

                        if (word.size()) {
                            int x1, y1, x2, y2;
                            word_i->BoundingBox(word_level, &x1, &y1, &x2, &y2);

                            words.push_back(Word(word, rx+  x1, ry + y1, rx + x2, ry + y2));
                            font = loadFontFitting(disp, DefaultScreen(disp), FONTNAME, 30, x2-x1, y2-y1, word.c_str());
                            for (int dx = -1; dx <= 1; dx++) {
                                for (int dy = -1; dy <= 1; dy++) {
                                    if (dx == 0 && dy == 0) continue; 
                                    XftDrawRect(draw, &color_darkBG, rx + x1, ry + y1, x2 - x1, y2-y1);
                                    XftDrawStringUtf8(draw, &color_stroke, font,
                                                    rx + x1 + dx, ry + y1 + dy + RESULT_SIZE,
                                                    (FcChar8*)word.c_str(), word.size());
                                }
                            }
                            XftDrawStringUtf8(draw, &color, font, rx + x1, ry + y1 + RESULT_SIZE,
                              (FcChar8*)word.c_str(), word.size());
                        }} while(word_i -> Next(word_level) && (word_i ->IsAtBeginningOf(tesseract::RIL_BLOCK)==false));
                        blocks.push_back(Block(bx1, by1, bx2, by2, words));
                    } while (ri->Next(block_level));

                }
                btns[0].SetPos(rx, ry + rh + 10);
                btns[1].SetPos(rx + 300, ry + rh + 10);
                btns[2].SetPos(rx + 600, ry + rh + 10);
                ready = 1;
            } else{
                int mx = ev.xbutton.x;
                int my = ev.xbutton.y;
                for (auto &b : btns) {
                if (mx >= b.x && mx <= b.x+b.w && my >= b.y && my <= b.y+b.h) {
                b.Call(disp, window);
                
                }
            }
            }
                
              break;
        }

        
        }
        if (ready){
            for (auto &b : btns) {
                b.Draw(disp, window, gc, DefaultScreen(disp), draw);
            }
            if(TRANSLATED){
                TRANSLATED=0;
                for(Block &b : blocks){
                    for(Word &w : b.words){
                        if (w.text == " ") XftDrawRect(draw, &color_darkBG, w.x1, w.y1, w.x2 - w.x1, w.y2-w.y1);
                        font = loadFontFitting(disp, DefaultScreen(disp), FONTNAME, 30, w.x2-w.x1, w.y2 - w.y1, w.text.c_str());
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dy = -1; dy <= 1; dy++) {
                                if (dx == 0 && dy == 0) continue; 
                                    XftDrawRect(draw, &color_darkBG, w.x1, w.y1, w.x2 - w.x1, w.y2-w.y1);
                                    XftDrawStringUtf8(draw, &color_stroke, font,
                                                    w.x1 + dx, w.y1 + dy + RESULT_SIZE,
                                                    (FcChar8*)w.text.c_str(), w.text.size());
                                }
                            }
                            XftDrawStringUtf8(draw, &color, font, w.x1, w.y1 + RESULT_SIZE,
                              (FcChar8*)w.text.c_str(), w.text.size());

                    }
                }
            }
        }

        XDrawRectangle(disp, window, gc, rect_x, rect_y, rect_w, rect_h);
        XFlush(disp);
    }
    /* clear the drawn rectangle */
    

}
