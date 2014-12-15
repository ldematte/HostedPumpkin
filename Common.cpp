
#include "Common.h"


bstr_t toBSTR(const std::string& s) {
   return bstr_t(s.c_str());
}

std::wstring CurrentDirectory() {
   wchar_t buffer[MAX_PATH + 1];
   ::GetCurrentDirectoryW(sizeof(buffer), buffer);
   return std::wstring(buffer);
}

std::wstring toWstring(const std::string& s) {
   std::wstring ws(s.size(), L' ');
   size_t charsConverted = 0;
   mbstowcs_s(&charsConverted, &ws[0], ws.capacity(), s.c_str(), s.size());
   ws.resize(charsConverted);
   return ws;
}

std::string toString(const std::wstring& ws) {
   std::string s(ws.size() + 1, ' ');
   size_t charsConverted = 0;
   wcstombs_s(&charsConverted, &s[0], s.capacity(), ws.c_str(), ws.size());
   s.resize(charsConverted);
   return s;
}

