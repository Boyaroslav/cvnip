#include <sys/socket.h>
#include<unistd.h>
#include <netdb.h>
#include<string>
#include<vector>

#include <iostream>


#define LibreTranslate 1           
#define API_KEY ""
#define PORT 5000
#define URL "localhost"

const int PROVIDER = LibreTranslate;

using namespace cv;
using namespace std;

string extractTranslation(const string &json) {
    const string key = "\"translatedText\":\"";
    size_t pos = json.find(key);
    if (pos == string::npos) return "";
    pos += key.size();
    size_t end = json.find("\"", pos);
    if (end == string::npos) return "";
    return json.substr(pos, end - pos);
}

string escapeJson(const string &s) {
    string r;
    for (char c : s) {
        if (c == '\"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else r += c;
    }
    return r;
}


string Translate(string q, string source, string target){ // source 'auto' for autodetect
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *server;
    struct sockaddr_in serv_addr;
    if (sfd < 0){cout<<"translate sock. bye\n"; close(sfd); exit(0);}
    server = gethostbyname(URL);
    if (server == NULL){
        cout<<"host. bye\n";
        close(sfd);
        exit(0);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(PORT);

    if (connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout<<"can't connect. bye\n";
        close(sfd);
        exit(1);
    }

    string body_ = "{\"q\": \"" + escapeJson(q) + "\", \"source\": \"" + source + "\", \"target\": \"" + target + "\"}";

    string body = "POST /translate HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body_.size()) + " \r\n"
        "Connection: close\r\n"
        "\r\n" + body_;
    const char *req = body.c_str();
    ssize_t snt = send(sfd, req, body.size(), 0);
    if (snt < 0){ cout<<"can't send. bye\n"; close(sfd);exit(0);}
    string ret;
    char ret_[1024];
    while ((snt = recv(sfd, ret_, body.size()-1, 0)) > 0){
        ret_[snt] = '\0';
        ret += string(ret_);
    }
    close(sfd);
    ret = extractTranslation(ret);
    std::cout<<ret<<"\n";
    return ret;
}


std::vector<std::string> Split(const string &s, char d) {
    std::vector<string> result;
    size_t start = 0;
    size_t pos = s.find(d, start);

    while (pos != string::npos) {
        result.push_back(s.substr(start, pos - start));
        start = pos + 1;
        pos = s.find(d, start);
    }

    result.push_back(s.substr(start));
    return result;
}
