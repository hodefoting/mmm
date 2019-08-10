
if sdl1.found()

mmm_sdl = executable('mmm.sdl',
      ['host.c', 'sdl.c', 'alsa-audio.c'],
      include_directories: [ rootInclude, mmmInclude ],
      link_with : mmm_lib,
      dependencies: [ sdl1, alsa, thread  ],
      install: true
)
endif

mmm_linux = executable('mmm.linux',
      ['host.c',
       'linux.c',
       'alsa-audio.c',
       'linux-evsource-kb.c',
       'linux-evsource-mice.c',
],
      include_directories: [ rootInclude, mmmInclude ],
      link_with : mmm_lib,
      dependencies: [ alsa, thread  ],
      install: true
)


mmm_kobo = executable('mmm.kobo',
      ['host.c',
       'kobo.c',
       'linux-evsource-ts.c',
       'linux-evsource-kb.c',
       'linux-evsource-mice.c',
],
      include_directories: [ rootInclude, mmmInclude ],
      link_with : mmm_lib,
      dependencies: [ alsa, thread  ],
      install: true
)


