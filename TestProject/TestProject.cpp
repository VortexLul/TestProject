#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>

using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

string ExtractFileNameFromUrl(const string& url) {

    size_t question_mark = url.find('?');
    string clean_url = (question_mark != string::npos) ?
        url.substr(0, question_mark) : url;


    size_t last_slash = clean_url.find_last_of('/');
    if (last_slash != string::npos && last_slash + 1 < clean_url.length()) {
        string filename = clean_url.substr(last_slash + 1);

        if (!filename.empty() && filename.find('.') != string::npos) {
            return filename;
        }
    }

    return "downloaded_file";
}




bool DowloadFunc(const string& url, string& folderPath) {

    CURL* curl;
    CURLcode res;
    string response;

    curl = curl_easy_init();
    if (!curl) {
        cerr << "Fail init ������ �������������" << endl;
        return false;
    }


    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");


    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "Fail DOWNL ������ ����������:" << curl_easy_strerror(res) << endl;
        curl_easy_cleanup(curl);
        return false;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);


    if (response_code != 200) {
        cerr << "Fail HTTP ������ HTTP ������" << response_code << endl;
        curl_easy_cleanup(curl);
        return false;
    }

    string filename = ExtractFileNameFromUrl(url);

    filesystem::path dirpath(folderPath);
    filesystem::path fullPath = dirpath / filename;

    filesystem::create_directories(dirpath);

    ofstream file(fullPath, ios::binary);
    if (!file.is_open()) {
        cerr << "Fail file ������ �������� �����: " << fullPath << endl;
        return false;
    }

    file.write(response.c_str(), response.size());
    file.close();

}

    //if (curl) {
    //    FILE* fp = nullptr;
    //    errno_t err = fopen_s(&fp, filename.c_str(), "wb");

    //    if (err != 0 || fp == nullptr) {
    //        cout << "�� ������� ������� ���� ��� ������" << endl;
    //        curl_easy_cleanup(curl);
    //        return 1;
    //    }
    // }

int main() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();

        string url, folderPath;

        cout << "������� URL �����:";
        getline(cin, url);

        cout << "������� ���� ��� ����������:";
        getline(cin, folderPath);

        if (url.empty() || folderPath.empty()) {
            cerr << "������: URL � ���� �� ����� ���� �������" << endl;
            return 1;
        }

        cout << "���������� �����..." << endl;
        cout << "URL: " << url << endl;
        cout << "����: " << folderPath << endl;


        if (DowloadFunc(url, folderPath)) {
            cout << "Suc�ess ���� �������� ������" << endl;
        } else {
            cerr << "Fail ������ ���������� �����" << endl;
            curl_global_cleanup();
            return 1;
        }

        curl_global_cleanup();

        return 0;

        // if (url.empty() || folderPath.empty()) {
        //     cerr << "URL � ���� �� ������ ���� �������" << endl;
        //     return 1;
        // }
        /////* if (DowloadFunc(url, folderPath)) {
        ////      cout << "���� ������� ������!" << endl;
        //// }
        //// else {
        ////     cerr << "������ ���������� �����" << endl;
        ////     curl_global_cleanup();
        ////     return 1;
        //// }*/
        //// curl_global_cleanup();
        //return 0;
}
   