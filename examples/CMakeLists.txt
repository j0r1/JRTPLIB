apply_include_paths("${JRTPLIB_INTERNAL_INCLUDES}")
apply_include_paths("${JRTPLIB_EXTERNAL_INCLUDES}")

foreach(IDX 1 2 3 4 5)
	add_executable(example${IDX} example${IDX}.cpp)
	target_link_libraries(example${IDX} jrtplib-static)
endforeach(IDX)

