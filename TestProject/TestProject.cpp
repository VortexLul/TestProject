#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>

using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

void DowloadFunc(const string& url, string& folderPath) {

    CURL* curl;
    CURLcode res;
    string response;
   
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);


    CURLcode res = curl_easy_perform(curl);

    /*if (!curl) {
        cerr << "Ошибка инициализации" << endl;
        return false;
    }*/

    //if (curl) {
    //    FILE* fp = nullptr;
    //    errno_t err = fopen_s(&fp, filename.c_str(), "wb");

    //    if (err != 0 || fp == nullptr) {
    //        cout << "Не удалось открыть файл для записи" << endl;
    //        curl_easy_cleanup(curl);
    //        return 1;
    //    }
    // }

    int main(); {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();
        string url, folderPath;

        string filename = "downloaded_image.png";

        cout << "Введите URL файла:";
        getline(cin, url);

        cout << "Укажите путь для сохранения:";
        getline(cin, folderPath);

        // if (url.empty() || folderPath.empty()) {
        //     cerr << "URL и путь не должны быть пустыми" << endl;
        //     return 1;
        // }
        /////* if (DowloadFunc(url, folderPath)) {
        ////     std::cout << "Файл успешно скачан!" << std::endl;
        //// }
        //// else {
        ////     std::cerr << "Ошибка скачивания файла" << std::endl;
        ////     curl_global_cleanup();
        ////     return 1;
        //// }*/
        //// curl_global_cleanup();
        //return 0;
}
   