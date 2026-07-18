def package_path($root):
  if type != "string" or . == "" or startswith("/") then .
  else $root + "/" + .
  end;

.listen_host = $listen_host
| .port = $port
| .worker_threads = $worker_threads
| .max_request_bytes = $max_request_bytes
| .max_image_pixels = $max_image_pixels
| .api_key = $api_key
| .runtime_root |= package_path($root)
| .model_manifest |= package_path($root)
| .web_root |= package_path($root)
| if (.runtime_library? | type) == "string" and .runtime_library != ""
  then .runtime_library |= package_path($root)
  else .
  end
