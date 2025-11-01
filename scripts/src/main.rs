use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process;

use naga::back::msl;
use naga::valid::{Capabilities, ValidationFlags, Validator};
use naga::{AddressSpace, ResourceBinding};

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() != 3 {
        eprintln!("Usage: {} <input.wgsl> <output.msl>", args[0]);
        process::exit(1);
    }

    let input_path = PathBuf::from(&args[1]);
    let output_path = PathBuf::from(&args[2]);

    if let Err(e) = convert_wgsl_to_msl(&input_path, &output_path) {
        eprintln!("Error: {}", e);
        process::exit(1);
    }
}

fn convert_wgsl_to_msl(input_path: &Path, output_path: &Path) -> Result<(), String> {
    // Read WGSL source
    let wgsl_source = fs::read_to_string(input_path)
        .map_err(|e| format!("Failed to read {}: {}", input_path.display(), e))?;

    // Parse WGSL
    let module = naga::front::wgsl::parse_str(&wgsl_source)
        .map_err(|e| format!("WGSL parse error: {}", e))?;

    // Validate module
    let mut validator = Validator::new(ValidationFlags::all(), Capabilities::all());
    let module_info = validator
        .validate(&module)
        .map_err(|e| format!("Validation error: {}", e))?;

    // Determine entrypoint name from filename
    let file_stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .ok_or("Invalid filename")?;

    let entrypoint_name = determine_entrypoint_name(file_stem);

    // Build binding map for all entry points
    // Map group(0) binding(0) to buffer(0)
    let mut per_entry_point_map = BTreeMap::new();

    for (ep_index, entry_point) in module.entry_points.iter().enumerate() {
        let mut resources = BTreeMap::new();
        let ep_info = module_info.get_entry_point(ep_index);

        // For each global variable in the module
        for (handle, global_var) in module.global_variables.iter() {
            // Check if this variable is used by this entry point
            if !ep_info[handle].is_empty() {
                // If the variable has a binding
                if let Some(ref binding) = global_var.binding {
                    let resource_binding = ResourceBinding {
                        group: binding.group,
                        binding: binding.binding,
                    };

                    // Map to Metal buffer slot
                    // We use buffer slots for uniform and storage buffers
                    let bind_target = match global_var.space {
                        AddressSpace::Uniform | AddressSpace::Storage { .. } => {
                            msl::BindTarget {
                                buffer: Some(binding.binding as u8),
                                texture: None,
                                sampler: None,
                                mutable: matches!(
                                    global_var.space,
                                    AddressSpace::Storage { access } if access.contains(naga::StorageAccess::STORE)
                                ),
                            }
                        }
                        _ => continue,
                    };

                    resources.insert(resource_binding, bind_target);
                }
            }
        }

        let entry_point_resources = msl::EntryPointResources {
            resources,
            push_constant_buffer: None,
            sizes_buffer: None,
        };

        per_entry_point_map.insert(entry_point.name.clone(), entry_point_resources);
    }

    // Configure MSL options
    let options = msl::Options {
        lang_version: (1, 0),
        per_entry_point_map,
        inline_samplers: Vec::new(),
        spirv_cross_compatibility: false,
        fake_missing_bindings: false,  // This is the key - we provide real bindings!
        bounds_check_policies: Default::default(),
        zero_initialize_workgroup_memory: true,
        force_loop_bounding: false,  // Don't inject loop bounding code
    };

    // Generate MSL
    let pipeline_options = msl::PipelineOptions {
        entry_point: None,  // Write all entry points
        allow_and_force_point_size: false,
        vertex_pulling_transform: false,
        vertex_buffer_mappings: Default::default(),
    };

    let (msl_source, _) = msl::write_string(&module, &module_info, &options, &pipeline_options)
        .map_err(|e| format!("MSL generation error: {:?}", e))?;

    // Rename entrypoint if needed
    let msl_final = if let Some(new_name) = entrypoint_name {
        rename_entrypoint(&msl_source, "main_", new_name)
    } else {
        msl_source
    };

    // Write output
    fs::write(output_path, msl_final)
        .map_err(|e| format!("Failed to write {}: {}", output_path.display(), e))?;

    Ok(())
}

fn determine_entrypoint_name(file_stem: &str) -> Option<&'static str> {
    // Determine target entrypoint name based on filename convention
    if file_stem.ends_with("_vertex") {
        if file_stem.contains("_overlay_") {
            Some("overlay_vertex")
        } else {
            Some("main_vertex")
        }
    } else if file_stem.ends_with("_fragment") {
        if file_stem.contains("_overlay_") {
            Some("overlay_fragment")
        } else {
            Some("main_fragment")
        }
    } else {
        None
    }
}

fn rename_entrypoint(msl_source: &str, old_name: &str, new_name: &str) -> String {
    // Replace function definition
    // Look for patterns like "vertex main_Output main_(" or "fragment main_Output main_("
    let vertex_pattern = format!("vertex main_Output {}(", old_name);
    let fragment_pattern = format!("fragment main_Output {}(", old_name);

    let result = msl_source.replace(&vertex_pattern, &format!("vertex main_Output {}(", new_name));
    let result = result.replace(&fragment_pattern, &format!("fragment main_Output {}(", new_name));

    result
}
