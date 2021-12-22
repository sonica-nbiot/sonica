project('sonica', ['c', 'cpp'],
  default_options : ['buildtype=debugoptimized'])
add_global_arguments('-march=native', language : ['c','cpp'])
add_global_arguments('-Wno-reorder', '-Wno-non-virtual-dtor', language : ['cpp'])

# Compiler and dependencies
cc = meson.get_compiler('c')
libm = cc.find_library('m')
pthread = dependency('threads')

fftw3f = dependency('fftw3f')

boost = dependency('boost', modules : ['program_options'])

sec_dep = dependency('', required : false)

polarssl = dependency('Polarssl', method : 'cmake', required : false, cmake_module_path : 'cmake-modules')
if (polarssl.found())
  sec_dep = polarssl
  add_global_arguments('-DHAVE_POLARSSL', language : ['c','cpp'])
else
  sec_dep = dependency('MbedTLS', method : 'cmake', cmake_module_path : 'cmake-modules')
  add_global_arguments('-DHAVE_MBEDTLS', language : ['c','cpp'])
endif

libconfigpp = dependency('libconfig++')

libsctp = dependency('libsctp')

zeromq = dependency('libzmq', required : false)
uhd = dependency('uhd', required : false)
bladerf = dependency('bladeRF', required : false)
soapysdr = dependency('SoapySDR', required : false)

rf_found = false

if zeromq.found() or uhd.found() or bladerf.found() or soapysdr.found()
  rf_found = true
endif

branch_cmd = run_command('scripts/get_gitbranch.sh')
rev_cmd = run_command('scripts/get_gitrev.sh')
buildinfo_cfg = configuration_data({
  'BUILD_MODE': get_option('buildtype'),
  'GIT_COMMIT_HASH': rev_cmd.stdout().strip(),
  'GIT_BRANCH': branch_cmd.stdout().strip()
})

version_cfg = configuration_data({
  'SONICA_VERSION_MAJOR' : 0,
  'SONICA_VERSION_MINOR' : 99,
  'SONICA_VERSION_PATCH' : 0,
  'SONICA_VERSION_STRING' : '0.99.0'
})


## SIMD Probing
have_sse = false
have_avx = false
have_avx2 = false
have_fma = false
have_avx512 = false

target_cpu = target_machine.cpu_family()
if target_cpu == 'x86' or target_cpu == 'x86_64'
  sse_probe_src = '''#include <emmintrin.h>
      #include <smmintrin.h>

      int main()
      {
      __m128i a = _mm_setzero_si128();
      __m128i b = _mm_minpos_epu16(a);
      return 0;
      }
      '''
  sse_result = cc.run(sse_probe_src, name : 'SSE probe', args : ['-msse4.1'])
  if sse_result.compiled() and sse_result.returncode() == 0
    have_sse = true
  endif

  avx_probe_src = '''#include <immintrin.h>
      int main()
      {
        __m256 a, b, c;
        const float src[8] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        float dst[8];
        a = _mm256_loadu_ps( src );
        b = _mm256_loadu_ps( src );
        c = _mm256_add_ps( a, b );
        _mm256_storeu_ps( dst, c );
        int i = 0;
        for( i = 0; i < 8; i++ ){
          if( ( src[i] + src[i] ) != dst[i] ){
            return -1;
          }
        }
        return 0;
      }
      '''
  avx_result = cc.run(avx_probe_src, name : 'AVX probe', args : ['-mavx'])
  if avx_result.compiled() and avx_result.returncode() == 0
    have_avx = true
  endif

  avx2_probe_src = '''#include <immintrin.h>
      int main()
      {
        __m256i a, b, c;
        const int src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        int dst[8];
        a =  _mm256_loadu_si256( (__m256i*)src );
        b =  _mm256_loadu_si256( (__m256i*)src );
        c = _mm256_add_epi32( a, b );
        _mm256_storeu_si256( (__m256i*)dst, c );
        int i = 0;
        for( i = 0; i < 8; i++ ){
          if( ( src[i] + src[i] ) != dst[i] ){
            return -1;
          }
        }
        return 0;
      }
      '''
  avx2_result = cc.run(avx2_probe_src, name : 'AVX2 probe', args : ['-mavx2'])
  if avx2_result.compiled() and avx2_result.returncode() == 0
    have_avx2 = true
  endif

  fma_probe_src = '''#include <immintrin.h>
      int main()
      {
        __m256 a, b, c, r;
        const float src[8] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        float dst[8];
        a = _mm256_loadu_ps( src );
        b = _mm256_loadu_ps( src );
        c = _mm256_loadu_ps( src );
        r = _mm256_fmadd_ps( a, b, c );
        _mm256_storeu_ps( dst, r );
        int i = 0;
        for( i = 0; i < 8; i++ ){
          if( ( src[i] * src[i] + src[i] ) != dst[i] ){
            return -1;
          }
        }
        return 0;
      }
      '''
  fma_result = cc.run(fma_probe_src, name : 'FMA probe', args : ['-mfma'])
  if fma_result.compiled() and fma_result.returncode() == 0
    have_fma = true
  endif

  avx512_probe_src = '''#include <immintrin.h>
      int main()
      {
        __m512i a, b, c;
        const int src[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8 , 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
        int dst[16];
        a =  _mm512_loadu_si512( (__m512i*)src );
        b =  _mm512_loadu_si512( (__m512i*)src );
        c = _mm512_add_epi32( a, b );
        _mm512_storeu_si512( (__m512i*)dst, c );
        int i = 0;
        for( i = 0; i < 16; i++ ){
          if( ( src[i] + src[i] ) != dst[i] ){
            return -1;
          }
        }
        return 0;
      }
      '''
  avx512_result = cc.run(avx512_probe_src, name : 'AVX512 probe',
    args : ['-mavx512f', '-mavx512cd', '-mavx512bw', '-mavx512dq'])
  if avx512_result.compiled() and avx512_result.returncode() == 0
    have_avx512 = true
  endif
endif

if have_avx2
  add_global_arguments('-mfpmath=sse', '-mavx2', '-DLV_HAVE_AVX2', '-DLV_HAVE_AVX', '-DLV_HAVE_SSE', language : ['c','cpp'])
elif have_avx
  add_global_arguments('-mfpmath=sse', '-mavx', '-DLV_HAVE_AVX', '-DLV_HAVE_SSE', language : ['c','cpp'])
elif have_sse
  add_global_arguments('-mfpmath=sse', '-msse4.1', '-DLV_HAVE_SSE', language : ['c','cpp'])
endif

if have_fma
  add_global_arguments('-mfma', '-DLV_HAVE_FMA', language : ['c','cpp'])
endif

if have_avx512
  add_global_arguments('-mavx512f', '-mavx512cd', '-mavx512bw', '-mavx512dq', '-DLV_HAVE_AVX512', language : ['c','cpp'])
endif

subdir('third_party')

subdir('include')

subdir('src')

subdir('sonica_enb')
