#pragma once
#include <windows.h>

inline bool chineseUI(LANGID language){return PRIMARYLANGID(language)==LANG_CHINESE;}
inline const wchar_t* uiText(bool chinese,const wchar_t* zh,const wchar_t* en){return chinese?zh:en;}
