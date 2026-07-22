add_library(cuexis_animation INTERFACE)
add_library(cuexis::animation ALIAS cuexis_animation)
target_link_libraries(cuexis_animation INTERFACE cuexis::core cuexis::world)
