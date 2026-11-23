#pragma once
#include <QCoreApplication>
#include "builtins/clipboard/clipboard-preferences.hpp"
#include "builtins/clipboard/history/clipboard-hold-paste.hpp"
#include "builtins/clipboard/history/clipboard-history-view-host.hpp"
#include "command/typed-command.hpp"
#include "vicinae.hpp"

class ClipboardHistoryCommand
    : public TypedViewCommand<ClipboardHistoryViewHost, ClipboardHistoryPreferences> {
  Q_DECLARE_TR_FUNCTIONS(ClipboardHistoryCommand)
  QString id() const override { return "history"; }
  QString name() const override { return tr("Clipboard History"); }
  QString description() const override {
    return tr("Browse your clipboard's history, pin, edit and remove entries.");
  }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(BuiltinIcon::CopyClipboard).setBackgroundTint(SemanticColor::Red);
  }

  void preferencesChanged(const ClipboardHistoryPreferences &preferences) const override {
    m_preferences = preferences;
  }

  bool shortcutPressed(const ApplicationContext &ctx) const override {
#ifdef Q_OS_MACOS
    if (m_preferences.holdToPaste) {
      return ClipboardHoldPaste::instance()->shortcutPressed(ctx, uniqueId());
    }
#endif
    return false;
  }

private:
  mutable ClipboardHistoryPreferences m_preferences;
};
