add_compile_definitions(
  BOOST_ALLOW_DEPRECATED_HEADERS
  BOOST_BIND_GLOBAL_PLACEHOLDERS
  )

include_directories(base)
include_directories(apps)

add_subdirectory(tin_creation)
add_subdirectory(apps)
add_subdirectory(dev-tests)
