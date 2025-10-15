#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>
#include <regex>


using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

string ExtractFileNameFromUrl(const string& url) {

    size_t question_mark = url.find('?');
    string clean_url = (question_mark != string::npos) ? url.substr(0, question_mark) : url;


    size_t last_slash = clean_url.find_last_of('/');
    if (last_slash != string::npos && last_slash + 1 < clean_url.length()) {
        string filename = clean_url.substr(last_slash + 1);

        if (!filename.empty() && filename.find('.') != string::npos) {
            return filename;
        }
    }

    return "downloaded_file";
}

struct ResponseData {
    string content;
    string contentDisposition;
    long responseCode;
};


string ExtractFileName(const string& contentDisposition) {
    if (contentDisposition.empty()) {
        return "";
    }

    size_t filename_pos = contentDisposition.find("filename=");
    if (filename_pos == string::npos) {
        return "";
    }

    filename_pos += 9;

    if (contentDisposition[filename_pos] == '\"' || contentDisposition[filename_pos] == '\'') {
        filename_pos++;
        size_t end_quote = contentDisposition.find(contentDisposition[filename_pos - 1], filename_pos);
        if (end_quote != string::npos) {
            return contentDisposition.substr(filename_pos, end_quote - filename_pos);
        }
    }
    else {
        size_t end_pos = contentDisposition.find(';', filename_pos);
        if (end_pos == string::npos) {
            end_pos = contentDisposition.find('\r', filename_pos);
        }
        if (end_pos == string::npos) {
            end_pos = contentDisposition.find('\n', filename_pos);
        }
        if (end_pos != string::npos) {
            return contentDisposition.substr(filename_pos, end_pos - filename_pos);
        }

    }
    return "";
}



string ReplaceUnvalidName(const string& filename) {
    if (filename.empty()) {
        return "dowloaded_file";
    }

    string replace = filename;

    regex invalid_chars("[<>:\"/\\\\|?*]");
    replace = regex_replace(replace, invalid_chars, "_");

    replace.erase(0, replace.find_last_not_of(" ."));
    replace.erase(replace.find_first_not_of(" .") + 1);

    if (replace.empty()) {
        return "download_file";
    }

    return replace;
}





bool DowloadFunc(const string & url, string & folderPath) {

        CURL* curl;
        CURLcode res;
        string response;

        curl = curl_easy_init();
        if (!curl) {
            cerr << "Fail init Ошибка инициализации" << endl;
            return false;
        }


        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");


        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "Fail DOWNL Ошибка скачивания:" << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            return false;
        }

        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);


        if (response_code != 200) {
            cerr << "Fail HTTP Ошибка HTTP ответа" << response_code << endl;
            curl_easy_cleanup(curl);
            return false;
        }

        string filename = ExtractFileNameFromUrl(url);

        filesystem::path dirpath(folderPath);
        filesystem::path fullPath = dirpath / filename;

        filesystem::create_directories(dirpath);

        ofstream file(fullPath, ios::binary);
        if (!file.is_open()) {
            cerr << "Fail file Ошибка создания файла: " << fullPath << endl;
            return false;
        }

        file.write(response.c_str(), response.size());
        file.close();

    }

    //if (curl) {
    //    FILE* fp = nullptr;
    //    errno_t err = fopen_s(&fp, filename.c_str(), "wb");

    //    if (err != 0 || fp == nullptr) {
    //        cout << "Не удалось открыть файл для записи" << endl;
    //        curl_easy_cleanup(curl);
    //        return 1;
    //    }
    // }
int main() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();

        string url, folderPath;

        cout << "Введите URL файла:";
        getline(cin, url);

        cout << "Укажите путь для сохранения:";
        getline(cin, folderPath);

        if (url.empty() || folderPath.empty()) {
            cerr << "Ошибка: URL и путь не могут быть пустыми" << endl;
            return 1;
        }

        cout << "Скачивание файла..." << endl;
        cout << "URL: " << url << endl;
        cout << "Путь: " << folderPath << endl;


        if (DowloadFunc(url, folderPath)) {
            cout << "Sucсess Файл успешное скачан" << endl;
        } else {
            cerr << "Fail Ошибка скачивания файла" << endl;
            curl_global_cleanup();
            return 1;
        }

        curl_global_cleanup();

        return 0;

        // if (url.empty() || folderPath.empty()) {
        //     cerr << "URL и путь не должны быть пустыми" << endl;
        //     return 1;
        // }
        /////* if (DowloadFunc(url, folderPath)) {
        ////      cout << "Файл успешно скачан!" << std:endl;
        //// }
        //// else {
        ////     std::cerr << "ошибка скачивания файла" << endl;
        ////     curl_global_cleanup();
        ////     return 1;
        //// }*/
        //// curl_global_cleanup();
        //return 0;
}
   