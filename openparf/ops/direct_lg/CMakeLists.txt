# direct legalization for LUT and FF
set(OP_NAME direct_lg)

file(GLOB CPP_SOURCES
  src/*.cpp
  )

set(TARGET_NAME ${OP_NAME})

set(INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${PROJECT_SOURCE_DIR}/thirdparty/lemon
  ${PROJECT_BINARY_DIR}/thirdparty/lemon
  ${Boost_INCLUDE_DIRS}
  )

set(LINK_LIBS database bookshelf ehbookshelf util lemon)

add_pytorch_extension(${TARGET_NAME}_cpp ${CPP_SOURCES}
  EXTRA_INCLUDE_DIRS ${INCLUDE_DIRS}
  EXTRA_LINK_LIBRARIES ${LINK_LIBS})
install(TARGETS ${TARGET_NAME}_cpp DESTINATION openparf/ops/${OP_NAME})

file(GLOB INSTALL_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.py)
install(FILES ${INSTALL_SRCS} DESTINATION openparf/ops/${OP_NAME})
