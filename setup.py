#!/usr/bin/python
 
from distutils.core import setup, Extension
import os
 
__version__ = '0.1'
 
define_macros = [('MODULE_VERSION', '"%s"' % __version__), ('DEBUG', None)]
 
geoquad_extension = Extension(
	name='geoquad',
	sources=['geoquad.c'],
	define_macros=define_macros,
)
 
setup(
	name			= 'geoquad',
	version			= __version__,
	author			= 'Evan Klitzke',
	author_email	= 'evan@eklitzke.org',
	description		= 'Fast geoquad operations',
	ext_modules		= [geoquad_extension]
)
