##Cmake template for service menus
 
cmake_minimum_required(VERSION 2.8)
#SET THE NAME FOR YOUR SERVICE MENU
PROJECT("ssmsd")
SET (CMAKE_PROJECT_NAME "ssmsd")

#find_package(avahi-client avahi-common REQUIRED)
 
#install the service menu on the correct folder
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib/systemd/system/ssmsd.service DESTINATION /etc/systemd/system )
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/var/log/ssmsd DESTINATION /var/log/ )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/var/ssmsd/database.db DESTINATION /var/ssmsd/ )

 
 #install the script where you should be
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/usr/bin/ssmsd
  DESTINATION bin)
 
SET(MAJOR_VERSION "1")
SET(MINOR_VERSION "0")
SET(PATCH_VERSION "0")
 
IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
INCLUDE(InstallRequiredSystemLibraries)
 
SET(CPACK_SET_DESTDIR "on")
SET(CPACK_PACKAGING_INSTALL_PREFIX "/tmp")
SET(CPACK_GENERATOR "RPM")
 
SET(CPACK_PACKAGE_DESCRIPTION "Simple Sensor Management System Daemon")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "It's a service for Simple Sensor Management System")
SET(CPACK_PACKAGE_VENDOR "Bartosz Stando")
SET(CPACK_PACKAGE_CONTACT "bartosz@stando.org")
SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")
SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
#demo dependencies
#SET(CPACK_DEBIAN_PACKAGE_DEPENDS " kate , plasma-desktop, libc6 (>= 2.1.3) ")
 
#dependencies for this service menu
SET(CPACK_RPM_PACKAGE_REQUIRES " avahi , avahi-libs ")
SET(CPACK_RPM_PACKAGE_RELOCATABLE "no")

#SET(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
#SET(CPACK_DEBIAN_PACKAGE_SECTION "network")
SET(CPACK_RPM_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
 
SET(CPACK_COMPONENTS_ALL Libraries ApplicationData)
INCLUDE(CPack)
 
ENDIF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
