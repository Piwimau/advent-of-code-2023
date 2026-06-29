project(
    'advent-of-code-2023',
    'c',
    default_options: [
        'c_std=c23',
        'buildtype=debug',
        'b_ndebug=if-release',
        'warning_level=3',
    ],
    meson_version: '>=1.1',
)

cc = meson.get_compiler('c')

if cc.get_id() == 'gcc'
    add_project_arguments(
        '-Wundef',
        '-Wshadow',
        '-Wconversion',
        '-Wdouble-promotion',
        '-Wcast-align',
        '-Wcast-qual',
        '-Wlogical-op',
        '-Wpointer-arith',
        '-Wformat=2',
        '-Wwrite-strings',
        '-Wmissing-declarations',
        '-Wmissing-include-dirs',
        '-Wmissing-prototypes',
        '-Wduplicated-cond',
        '-Wduplicated-branches',
        language: 'c',
    )
    if get_option('native')
        add_project_arguments('-march=native', '-mtune=native', language: 'c')
    endif
endif

m_dep = cc.find_library('m', required: false)
scu_dep = dependency('scu', fallback: ['scu', 'scu_dep'], static: true)

sh = find_program('sh')

days = [
    'day-01-trebuchet',
    'day-02-cube-conundrum',
    'day-03-gear-ratios',
    'day-04-scratchcards',
    'day-05-if-you-give-a-seed-a-fertilizer',
    'day-06-wait-for-it',
    'day-07-camel-cards',
    'day-08-haunted-wasteland',
    'day-09-mirage-maintenance',
    'day-10-pipe-maze',
    'day-11-cosmic-expansion',
    'day-12-hot-springs',
    'day-13-point-of-incidence',
    'day-14-parabolic-reflector-dish',
    'day-15-lens-library',
    'day-16-the-floor-will-be-lava',
    'day-17-clumsy-crucible',
    'day-18-lavaduct-lagoon',
    'day-19-aplenty',
    'day-20-pulse-propagation',
    'day-21-step-counter',
    'day-22-sand-slabs',
    'day-23-a-long-walk',
    'day-24-never-tell-me-the-odds',
    'day-25-snowverload',
]

foreach day : days
    exe = executable(
        day,
        day / 'src' / 'main.c',
        dependencies: [m_dep, scu_dep],
    )
    run_target(
        'run-' + day,
        command: [
            sh,
            '-c',
            exe.full_path()
                + ' < '
                + meson.project_source_root() / day / 'resources' / 'input.txt',
        ],
        depends: exe,
    )
endforeach