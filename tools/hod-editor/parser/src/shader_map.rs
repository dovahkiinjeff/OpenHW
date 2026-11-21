use std::collections::HashMap;
use std::fs;
use std::path::Path;

/// Represents a shader mapping from SHADERS.MAP
#[derive(Debug, Clone)]
pub struct ShaderMapping {
    /// Pipeline names (comma-separated, e.g., "ship,matte,matte2s")
    pub pipeline_names: Vec<String>,
    /// Shader parameters
    pub parameters: Vec<ShaderParameter>,
}

/// Represents a shader parameter (e.g., $diffuse, $glow)
#[derive(Debug, Clone)]
pub struct ShaderParameter {
    /// Parameter name (e.g., "$diffuse", "$glow")
    pub name: String,
    /// Texture format (e.g., "DXT1", "DXT5", "8888")
    pub format: String,
    /// Default values (e.g., "1 1 1 1")
    pub default_values: Vec<f32>,
    /// Channel mappings
    pub channel_mappings: Vec<ChannelMapping>,
}

/// Represents a channel mapping (e.g., "DIFF = R G B 1")
#[derive(Debug, Clone)]
pub struct ChannelMapping {
    /// Texture role (e.g., "DIFF", "GLOW", "TEAM")
    pub texture_role: String,
    /// Channel mapping string (e.g., "R G B 1")
    pub channels: String,
}

/// Parsed SHADERS.MAP data
#[derive(Debug, Clone)]
pub struct ShadersMap {
    /// All shader mappings indexed by pipeline name
    pub mappings: HashMap<String, ShaderMapping>,
    /// Default mapping for unknown pipelines
    pub default_mapping: Option<ShaderMapping>,
}

impl ShadersMap {
    /// Parse SHADERS.MAP file from path
    pub fn from_file(path: &Path) -> Result<Self, String> {
        let content =
            fs::read_to_string(path).map_err(|e| format!("Failed to read SHADERS.MAP: {}", e))?;
        Self::parse(&content)
    }

    /// Parse SHADERS.MAP content
    pub fn parse(content: &str) -> Result<Self, String> {
        let mut mappings = HashMap::new();
        let mut current_mapping: Option<ShaderMapping> = None;

        for line in content.lines() {
            let line = line.trim();

            // Skip comments and empty lines
            if line.starts_with('#') || line.is_empty() {
                continue;
            }

            // Handle pipeline block (+pipeline_name)
            if line.starts_with('+') {
                // Save previous mapping
                if let Some(mapping) = current_mapping.take() {
                    for name in &mapping.pipeline_names {
                        mappings.insert(name.clone(), mapping.clone());
                    }
                }

                // Parse pipeline names (comma-separated)
                let pipeline_names: Vec<String> = line[1..]
                    .split(',')
                    .map(|s| s.trim().to_string())
                    .filter(|s| !s.is_empty())
                    .collect();

                if !pipeline_names.is_empty() {
                    current_mapping = Some(ShaderMapping {
                        pipeline_names,
                        parameters: Vec::new(),
                    });
                }
                continue;
            }

            // Handle parameter definition ($param[FORMAT] = default_values)
            if line.starts_with('$') {
                if let Some(ref mut mapping) = current_mapping {
                    if let Some(param) = Self::parse_parameter(line) {
                        mapping.parameters.push(param);
                    }
                }
                continue;
            }

            // Handle channel mapping (TEXTURE_ROLE = channel_mapping)
            if let Some(ref mut mapping) = current_mapping {
                if let Some(last_param) = mapping.parameters.last_mut() {
                    if let Some(channel) = Self::parse_channel_mapping(line) {
                        last_param.channel_mappings.push(channel);
                    }
                }
            }
        }

        // Save last mapping
        if let Some(mapping) = current_mapping {
            for name in &mapping.pipeline_names {
                mappings.insert(name.clone(), mapping.clone());
            }
        }

        Ok(ShadersMap {
            mappings,
            default_mapping: None,
        })
    }

    /// Parse parameter line: $diffuse[DXT1] = 1 1 1 1
    fn parse_parameter(line: &str) -> Option<ShaderParameter> {
        let line = line.trim();
        if !line.starts_with('$') {
            return None;
        }

        // Find parameter name
        let param_end = line.find('[')?;
        let param_name = line[1..param_end].to_string();

        // Find format
        let format_start = param_end + 1;
        let format_end = line.find(']')?;
        let format = line[format_start..format_end].to_string();

        // Find default values
        let default_start = line.find('=')? + 1;
        let default_str = line[default_start..].trim();
        let default_values: Vec<f32> = default_str
            .split_whitespace()
            .filter_map(|s| s.parse().ok())
            .collect();

        Some(ShaderParameter {
            name: param_name,
            format,
            default_values,
            channel_mappings: Vec::new(),
        })
    }

    /// Parse channel mapping line: DIFF = R G B 1
    fn parse_channel_mapping(line: &str) -> Option<ChannelMapping> {
        let line = line.trim();
        if line.is_empty() || line.starts_with('$') || line.starts_with('+') {
            return None;
        }

        // Find texture role and channel mapping
        let parts: Vec<&str> = line.splitn(2, '=').collect();
        if parts.len() != 2 {
            return None;
        }

        let texture_role = parts[0].trim().to_string();
        let channels = parts[1].trim().to_string();

        // Validate texture role (should be uppercase letters)
        if texture_role.is_empty()
            || !texture_role
                .chars()
                .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit())
        {
            return None;
        }

        Some(ChannelMapping {
            texture_role,
            channels,
        })
    }

    /// Get shader mapping for a pipeline name
    pub fn get_mapping(&self, pipeline_name: &str) -> Option<&ShaderMapping> {
        self.mappings.get(pipeline_name)
    }

    /// Get all pipeline names that use a specific shader
    pub fn get_pipelines_for_shader(&self, shader_name: &str) -> Vec<&str> {
        self.mappings
            .keys()
            .filter(|name| name.contains(shader_name))
            .map(|s| s.as_str())
            .collect()
    }

    /// Auto-detect shader type from texture names
    pub fn detect_shader_type(textures: &[String]) -> String {
        let has_diff = textures.iter().any(|t| t.to_uppercase().contains("_DIFF"));
        let has_glow = textures.iter().any(|t| t.to_uppercase().contains("_GLOW"));
        let has_team = textures.iter().any(|t| t.to_uppercase().contains("_TEAM"));
        let has_spec = textures.iter().any(|t| t.to_uppercase().contains("_SPEC"));

        if has_team {
            "ship".to_string()
        } else if has_glow && has_spec {
            "shipglow".to_string()
        } else if has_glow {
            "ship".to_string() // Default to ship for glow-only
        } else if has_diff {
            "matte".to_string()
        } else {
            "ship".to_string() // Default fallback
        }
    }

    /// Map textures to shader parameters
    pub fn map_textures(
        &self,
        textures: &[String],
        shader_type: &str,
    ) -> Vec<(String, String, String)> {
        let mut mappings = Vec::new();

        if let Some(mapping) = self.get_mapping(shader_type) {
            for texture in textures {
                let texture_upper = texture.to_uppercase();

                // Find matching parameter based on texture name
                for param in &mapping.parameters {
                    let param_upper = param.name.to_uppercase();

                    // Check if texture name contains parameter name (e.g., _DIFF in $diffuse)
                    if texture_upper.contains(&param_upper[1..]) {
                        // Skip '$' prefix
                        mappings.push((texture.clone(), param.name.clone(), param.format.clone()));
                        break;
                    }
                }
            }
        }

        mappings
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_ship_shader() {
        let content = r#"
# Common ship shader, matte, etc
+ship,matte,matte2s,monolith,megalith,fxMatte
    $diffuse[DXT1] = 1 1 1 1
        DIFF = R G B 1
    $glow[DXT1]= 0 0 0 1
        GLOW = G G G G
        SPEC = B B B B
        REFL = R R R R
    $team[DXT1] = 1 1 0 1
        TEAM = 1 1 1 r
        STRP = 1 1 1 g
        PAIN = 1 1 1 b
    $normal[DXT1]= 5 5 1 1
        NORM[B] = R G B 1
"#;

        let shaders_map = ShadersMap::parse(content).unwrap();

        // Check that ship shader was parsed
        assert!(shaders_map.get_mapping("ship").is_some());
        assert!(shaders_map.get_mapping("matte").is_some());

        let ship = shaders_map.get_mapping("ship").unwrap();
        assert_eq!(ship.parameters.len(), 4);

        // Check diffuse parameter
        let diffuse = &ship.parameters[0];
        assert_eq!(diffuse.name, "diffuse");
        assert_eq!(diffuse.format, "DXT1");
        assert_eq!(diffuse.default_values, vec![1.0, 1.0, 1.0, 1.0]);
        assert_eq!(diffuse.channel_mappings.len(), 1);
        assert_eq!(diffuse.channel_mappings[0].texture_role, "DIFF");
        assert_eq!(diffuse.channel_mappings[0].channels, "R G B 1");
    }

    #[test]
    fn test_detect_shader_type() {
        let textures_diff = vec!["Pebble_DIFF.TGA".to_string()];
        let textures_glow = vec!["Pebble_DIFF.TGA".to_string(), "Pebble_GLOW.TGA".to_string()];
        let textures_team = vec![
            "Pebble_DIFF.TGA".to_string(),
            "Pebble_GLOW.TGA".to_string(),
            "Pebble_TEAM.TGA".to_string(),
        ];

        assert_eq!(ShadersMap::detect_shader_type(&textures_diff), "matte");
        assert_eq!(ShadersMap::detect_shader_type(&textures_glow), "ship");
        assert_eq!(ShadersMap::detect_shader_type(&textures_team), "ship");
    }
}
