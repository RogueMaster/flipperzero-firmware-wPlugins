#include <ESPAsyncWebServer.h>

AsyncWebHeader::AsyncWebHeader(const String& data) {
  if (!data)
    return;
  int index = data.indexOf(':');
  if (index < 0)
    return;
  _name = data.substring(0, index);
  _value = data.substring(index + 2);
}

String AsyncWebHeader::toString() const {
  String str;
  str.reserve(_name.length() + _value.length() + 2);
  str.concat(_name);
  str.concat((char)0x3a);
  str.concat((char)0x20);
  str.concat(_value);
  str.concat(asyncsrv::T_rn);
  return str;
}
