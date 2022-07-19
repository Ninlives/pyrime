from distutils.core import setup, Extension

rime_module = Extension('rime',
        libraries = ['rime'],
        sources = ['rimemodule.c'])

setup (name = 'pyrime',
        version = '0.1',
        description = 'Python rime utilities',
        ext_modules = [rime_module])
