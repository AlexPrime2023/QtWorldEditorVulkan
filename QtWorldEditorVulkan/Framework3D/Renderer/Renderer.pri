!RENDERER_PRI {

CONFIG += RENDERER_PRI

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/renderer.cpp \
	$$PWD/vkmaterialsystem.cpp \
	$$PWD/vkuploadcontext.cpp \
	$$PWD/vkgpuresourcecache.cpp \
	$$PWD/vkutils.cpp

HEADERS += \
    $$PWD/renderer.h \
	$$PWD/vkmaterialsystem.h \
	$$PWD/vkuploadcontext.h \
	$$PWD/vkrenderitem.h \
	$$PWD/vkgpuresources.h \
	$$PWD/vkgpuresourcecache.h \
	$$PWD/vkutils.h
	
        RESOURCES += $$PWD/assets.qrc

        Shaders = shaders/shader.vert shaders/shader.frag
	for (shader, Shaders) {
	        exists($$_PRO_FILE_PWD_/$${shader}) {
		        message(Compiling Spir-V $$_PRO_FILE_PWD_/$${shader})
			ERROR = $$system(glslangValidator -V -o $$_PRO_FILE_PWD_/$${shader}.spv $$_PRO_FILE_PWD_/$${shader})
			message($$ERROR)
		}
	}
}
