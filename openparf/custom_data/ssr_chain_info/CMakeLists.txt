# ssr chain info, specially for xarch benchmark.
set(OP_NAME ssr_chain_info)

file(GLOB CPP_SOURCES
  src/*.cpp
  )

set(TARGET_NAME ${OP_NAME})

set(INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${Boost_INCLUDE_DIRS}
  )

set(LINK_LIBS database bookshelf ehbookshelf util)

add_pytorch_extension(${TARGET_NAME}_cpp ${CPP_SOURCES}
  EXTRA_INCLUDE_DIRS ${INCLUDE_DIRS}
  EXTRA_LINK_LIBRARIES ${LINK_LIBS})
install(TARGETS ${TARGET_NAME}_cpp DESTINATION openparf/custom_data/${OP_NAME})

file(GLOB INSTALL_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.py)
install(FILES ${INSTALL_SRCS} DESTINATION openparf/custom_data/${OP_NAME})