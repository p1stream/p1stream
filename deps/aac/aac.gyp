{
    'variables': {
        'aac_include_dirs': [
            'aac/libFDK/include',
            'aac/libSYS/include',
            'aac/libMpegTPEnc/include',
            'aac/libSBRenc/include',
            'aac/libAACenc/include'
        ]
    },
    'target_defaults': {
        'include_dirs': ['<@(aac_include_dirs)'],
        'all_dependent_settings': {
            'include_dirs': ['<@(aac_include_dirs)']
        },
        'conditions': [
            ['OS == "mac"', {
                'xcode_settings': {
                    # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
                    'OTHER_CFLAGS': ['-mmacosx-version-min=10.8', '-std=c++98', '-stdlib=libc++'],
                    'WARNING_CFLAGS': ['-w'],
                    'WARNING_CFLAGS!': ['-Wall', '-Wendif-labels', '-W', '-Wno-unused-parameter']
                }
            }],
            ['OS == "linux"', {
                'cflags': ['-std=c++98', '-w'],
                'cflags!': [
                    '-Wextra', '-Wall', '-Wno-unused-parameter',
                    '-fno-omit-frame-pointer',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-fno-tree-vrp',
                    '-fno-tree-sink'
                ]
            }]
        ]
    },
    'targets': [
        {
            'target_name': 'libFDK',
            'type': 'static_library',
            'sources': [
                'aac/libFDK/src/autocorr2nd.cpp',
                'aac/libFDK/src/dct.cpp',
                'aac/libFDK/src/FDK_bitbuffer.cpp',
                'aac/libFDK/src/FDK_core.cpp',
                'aac/libFDK/src/FDK_crc.cpp',
                'aac/libFDK/src/FDK_hybrid.cpp',
                'aac/libFDK/src/FDK_tools_rom.cpp',
                'aac/libFDK/src/FDK_trigFcts.cpp',
                'aac/libFDK/src/fft.cpp',
                'aac/libFDK/src/fft_rad2.cpp',
                'aac/libFDK/src/fixpoint_math.cpp',
                'aac/libFDK/src/mdct.cpp',
                'aac/libFDK/src/qmf.cpp',
                'aac/libFDK/src/scale.cpp'
            ]
        },
        {
            'target_name': 'libSYS',
            'type': 'static_library',
            'sources': [
                'aac/libSYS/src/cmdl_parser.cpp',
                'aac/libSYS/src/conv_string.cpp',
                'aac/libSYS/src/genericStds.cpp',
                'aac/libSYS/src/wav_file.cpp'
            ]
        },
        {
            'target_name': 'libMpegTPEnc',
            'type': 'static_library',
            'sources': [
                'aac/libMpegTPEnc/src/tpenc_adif.cpp',
                'aac/libMpegTPEnc/src/tpenc_adts.cpp',
                'aac/libMpegTPEnc/src/tpenc_asc.cpp',
                'aac/libMpegTPEnc/src/tpenc_latm.cpp',
                'aac/libMpegTPEnc/src/tpenc_lib.cpp'
            ]
        },
        {
            'target_name': 'libSBRenc',
            'type': 'static_library',
            'sources': [
                'aac/libSBRenc/src/bit_sbr.cpp',
                'aac/libSBRenc/src/code_env.cpp',
                'aac/libSBRenc/src/env_bit.cpp',
                'aac/libSBRenc/src/env_est.cpp',
                'aac/libSBRenc/src/fram_gen.cpp',
                'aac/libSBRenc/src/invf_est.cpp',
                'aac/libSBRenc/src/mh_det.cpp',
                'aac/libSBRenc/src/nf_est.cpp',
                'aac/libSBRenc/src/ps_bitenc.cpp',
                'aac/libSBRenc/src/ps_encode.cpp',
                'aac/libSBRenc/src/ps_main.cpp',
                'aac/libSBRenc/src/resampler.cpp',
                'aac/libSBRenc/src/sbr_encoder.cpp',
                'aac/libSBRenc/src/sbr_misc.cpp',
                'aac/libSBRenc/src/sbr_ram.cpp',
                'aac/libSBRenc/src/sbr_rom.cpp',
                'aac/libSBRenc/src/sbrenc_freq_sca.cpp',
                'aac/libSBRenc/src/ton_corr.cpp',
                'aac/libSBRenc/src/tran_det.cpp'
            ]
        },
        {
            'target_name': 'libAACenc',
            'type': 'static_library',
            'sources': [
                'aac/libAACenc/src/aacenc.cpp',
                'aac/libAACenc/src/aacenc_hcr.cpp',
                'aac/libAACenc/src/aacenc_lib.cpp',
                'aac/libAACenc/src/aacenc_pns.cpp',
                'aac/libAACenc/src/aacEnc_ram.cpp',
                'aac/libAACenc/src/aacEnc_rom.cpp',
                'aac/libAACenc/src/aacenc_tns.cpp',
                'aac/libAACenc/src/adj_thr.cpp',
                'aac/libAACenc/src/band_nrg.cpp',
                'aac/libAACenc/src/bandwidth.cpp',
                'aac/libAACenc/src/bit_cnt.cpp',
                'aac/libAACenc/src/bitenc.cpp',
                'aac/libAACenc/src/block_switch.cpp',
                'aac/libAACenc/src/channel_map.cpp',
                'aac/libAACenc/src/chaosmeasure.cpp',
                'aac/libAACenc/src/dyn_bits.cpp',
                'aac/libAACenc/src/grp_data.cpp',
                'aac/libAACenc/src/intensity.cpp',
                'aac/libAACenc/src/line_pe.cpp',
                'aac/libAACenc/src/metadata_compressor.cpp',
                'aac/libAACenc/src/metadata_main.cpp',
                'aac/libAACenc/src/ms_stereo.cpp',
                'aac/libAACenc/src/noisedet.cpp',
                'aac/libAACenc/src/pnsparam.cpp',
                'aac/libAACenc/src/pre_echo_control.cpp',
                'aac/libAACenc/src/psy_configuration.cpp',
                'aac/libAACenc/src/psy_main.cpp',
                'aac/libAACenc/src/qc_main.cpp',
                'aac/libAACenc/src/quantize.cpp',
                'aac/libAACenc/src/sf_estim.cpp',
                'aac/libAACenc/src/spreading.cpp',
                'aac/libAACenc/src/tonality.cpp',
                'aac/libAACenc/src/transform.cpp'
            ]
        }
    ]
}
