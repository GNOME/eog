#!/bin/sh

xgettext --default-domain=eog --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f eog.po \
   || ( rm -f ./eog.pot \
    && mv eog.po ./eog.pot )
