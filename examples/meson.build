
math = cc.find_library('m',   required: false)


examples = [
 { 'name': 'audio', 'deps':[math], },
 { 'name': 'fragment-400x300', },
 { 'name': 'fragment-full', },
 { 'name': 'nano-test2', },
 { 'name': 'raw-client', },
 { 'name': 'raw-client-tool', },
]

foreach example : examples
  example_name = example.get('name')
  example_srcs = example.get('srcs', example_name + '.c')
  example_deps = example.get('deps', [])

  exe = executable(example_name, example_srcs, dependencies: [
     example_deps,
  ],
  include_directories : [mmmInclude,],
  link_with: [ mmm_lib ],
  install : false,)


endforeach
