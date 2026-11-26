#pragma once

#include <windows.h>
#include <cctype>

namespace jianhuan {

inline bool ShouldEatKeyPolicy(bool ascii_mode, bool composing, bool ascii_punct,
                               UINT vk, bool ctrl, bool alt, bool win, int keycode) {
  if (ascii_mode) return false;
  if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_CAPITAL ||
      vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS) {
    return false;
  }
  if (ctrl || alt || win) return false;
  if (keycode >= '0' && keycode <= '9') return composing;
  switch (vk) {
    case VK_BACK:
    case VK_DELETE:
    case VK_ESCAPE:
    case VK_RETURN:
    case VK_SPACE:
    case VK_TAB:
    case VK_HOME:
    case VK_END:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_OEM_PLUS:
    case VK_OEM_MINUS:
    case VK_ADD:
    case VK_SUBTRACT:
      return composing;
    default:
      break;
  }
  // Paging/editing keys above win over ascii_punct; only plain punctuation
  // passes through when English punctuation is enabled.
  if (keycode >= 0x20 && keycode <= 0x7e && keycode != ' ' &&
      !std::isalnum(static_cast<unsigned char>(keycode)) && ascii_punct) {
    return false;
  }
  // Only swallow printable text keys (to start/continue composition).
  // Function keys, media keys, and app hotkeys (e.g. Shift+F6) pass through.
  return keycode >= 0x21 && keycode <= 0x7e;
}

}  // namespace jianhuan