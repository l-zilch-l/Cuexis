add_library(cuexis_particles INTERFACE)
add_library(cuexis::particles ALIAS cuexis_particles)
target_link_libraries(cuexis_particles INTERFACE cuexis::core cuexis::world cuexis::render)
