#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFERSIZE 8192

const char* bufsocket = "/tmp/bufcvnip.sock";

std::atomic<bool> running(true);
std::string clipboardText;
std::mutex textMutex;


void ClipboardLoop(Display* dpy, Window win) {
    Atom clipboard = XInternAtom(dpy, "CLIPBOARD", False);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    Atom targets = XInternAtom(dpy, "TARGETS", False);

    XSetSelectionOwner(dpy, clipboard, win, CurrentTime);

    if (XGetSelectionOwner(dpy, clipboard) != win) {
        std::cerr << "Не удалось захватить CLIPBOARD\n";
        return;
    }

    XEvent ev;
    while (running) {
        XNextEvent(dpy, &ev);

        if (ev.type == SelectionRequest) {
            XSelectionRequestEvent *req = &ev.xselectionrequest;
            XEvent respond{};
            respond.xselection.type = SelectionNotify;
            respond.xselection.display = req->display;
            respond.xselection.requestor = req->requestor;
            respond.xselection.selection = req->selection;
            respond.xselection.time = req->time;
            respond.xselection.target = req->target;
            respond.xselection.property = req->property;

            if (req->target == targets) {
                Atom available[2] = { utf8, XA_STRING };
                XChangeProperty(dpy, req->requestor, req->property,
                                XA_ATOM, 32, PropModeReplace,
                                (unsigned char*)available, 2);
            } else if (req->target == utf8 || req->target == XA_STRING) {
                std::lock_guard<std::mutex> lock(textMutex);
                XChangeProperty(dpy, req->requestor, req->property,
                                req->target, 8, PropModeReplace,
                                (unsigned char*)clipboardText.c_str(),
                                clipboardText.size());
            } else {
                respond.xselection.property = None;
            }

            XSendEvent(dpy, req->requestor, True, 0, &respond);
            XFlush(dpy);
        }
    }
}

void Copy(std::string& text) {
    std::lock_guard<std::mutex> lock(textMutex);
    clipboardText = text;
}

void Copy(char* text){
    std::lock_guard<std::mutex> lock(textMutex);
    clipboardText.assign(text, BUFFERSIZE);
}
void Append(char* text, ssize_t len){
    std::lock_guard<std::mutex> lock(textMutex);
    clipboardText.append(text, len);
}

void Clear(char *text){
    for(int i=0; i<BUFFERSIZE;i++){text[i] = '\0';}
    std::lock_guard<std::mutex> lock(textMutex);
    clipboardText.clear();
}

void Update(Display* dpy, Window win){
    Atom XA_CLIPBOARD = XInternAtom(dpy, "CLIPBOARD", False);
    XSetSelectionOwner(dpy, XA_CLIPBOARD, win, CurrentTime);
    XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);
    if (XGetSelectionOwner(dpy, XA_CLIPBOARD) != win) {
        std::cerr << "not owned.\n";
    }

}

int main() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        std::cerr << "Не удалось открыть дисплей\n";
        return 1;
    }

    XSetWindowAttributes attr{};
    attr.event_mask = 0;

    Window win = XCreateWindow(
        dpy,
        DefaultRootWindow(dpy),
        0, 0,     
        1, 1,       
        0,           
        0,             
        InputOnly,       
        CopyFromParent,  
        0,               
        &attr            
    );

    clipboardText = "librejokes.ru - subscribe";
    std::thread t([&]() { ClipboardLoop(dpy, win); });


    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd <0){t.join(); std::cout<<"busy. bye\n"; return 0;}
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, bufsocket, sizeof(addr.sun_path)-1);
    unlink(addr.sun_path);

    if (bind(
        sfd, 
        (struct sockaddr*)&addr, sizeof(addr))
        < 0){
            std::cout<<"bind. bye\n"; close(sfd); return 0;
        }
    if(listen(sfd, 1)){std::cout<<"listen. bye\n"; close(sfd);return 0;}
    char buf[BUFFERSIZE];

    while(running){
        int clfd = accept(sfd, nullptr, nullptr);

        Clear(buf);

        if (clfd < 0) continue;
        ssize_t lng;

        while (lng = read(clfd, buf, BUFFERSIZE-1)){
            Append(buf, lng);
        }

        Update(dpy, win);

        close(clfd);
    }
    close(sfd);


    running = false;
    XCloseDisplay(dpy);
    t.join();
    return 0;
}