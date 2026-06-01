#!/usr/bin/env bash
set -e

$EXTRACTRC applet/metadata.json >> rc.cpp
$XGETTEXT applet/contents/config/config.qml \
    applet/contents/ui/DictionaryPopup.qml \
    applet/contents/ui/configTranslation.qml \
    src/translator.cpp \
    -o "$podir/plasma_applet_com.github.LXYan2333.swan-dict.pot"
rm -f rc.cpp
