#!/bin/bash

root_dir=$(dirname "$0")
main=$(find "$root_dir" -name "main")

"$main" make_base "$root_dir/tests/test_t1_base.json" && \
"$main" process_requests "$root_dir/tests/test_t1_stat.json" > "$root_dir/tmp_out" && \
diff -qZ "$root_dir/tests/test_t1_out" "$root_dir/tmp_out"

"$main" make_base "$root_dir/tests/test_t2_base.json" && \
"$main" process_requests "$root_dir/tests/test_t2_stat.json" > "$root_dir/tmp_out" && \
diff -qZ "$root_dir/tests/test_t2_out" "$root_dir/tmp_out"

rm "$root_dir/tmp_out"
