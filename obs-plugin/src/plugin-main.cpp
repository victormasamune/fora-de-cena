#include <obs-module.h>
#include <obs.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("fora_de_cena_obs", "pt-BR")

namespace {
constexpr const char *source_id = "fora_de_cena_webcam_filter";

struct FilterData {
    obs_source_t *context = nullptr;
    gs_effect_t *effect = nullptr;
    bool enabled = true;
    int effect_mode = 0;
};

void *filter_create(obs_data_t *settings, obs_source_t *context)
{
    auto *data = new FilterData();
    data->context = context;
    data->effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    data->enabled = obs_data_get_bool(settings, "enabled");
    data->effect_mode = static_cast<int>(obs_data_get_int(settings, "effect_mode"));
    return data;
}

void filter_destroy(void *opaque) { delete static_cast<FilterData *>(opaque); }

void filter_update(void *opaque, obs_data_t *settings)
{
    auto *data = static_cast<FilterData *>(opaque);
    data->enabled = obs_data_get_bool(settings, "enabled");
    data->effect_mode = static_cast<int>(obs_data_get_int(settings, "effect_mode"));
}

void filter_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "enabled", true);
    obs_data_set_default_int(settings, "effect_mode", 0);
}

obs_properties_t *filter_properties(void *)
{
    obs_properties_t *properties = obs_properties_create();
    obs_properties_add_bool(properties, "enabled", "Ativar filtro");
    obs_property_t *mode = obs_properties_add_list(
        properties, "effect_mode", "Efeito", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode, "Teleporte suave", 0);
    obs_property_list_add_int(mode, "Teleporte anime", 1);
    obs_property_list_add_int(mode, "Fumaça", 2);
    obs_property_list_add_int(mode, "Saída digital", 3);
    obs_property_list_add_int(mode, "Virar poeira", 4);
    obs_property_list_add_int(mode, "Saída de cena", 5);
    obs_property_list_add_int(mode, "Portal", 6);
    obs_property_list_add_int(mode, "Instantâneo", 7);
    obs_properties_add_text(properties, "prototype_notice",
        "Estado do desenvolvimento", OBS_TEXT_INFO);
    return properties;
}

void filter_video_render(void *opaque, gs_effect_t *effect)
{
    auto *data = static_cast<FilterData *>(opaque);
    if (!data || !data->context) return;
    if (!data->enabled) {
        obs_source_skip_video_filter(data->context);
        return;
    }

    // Initial milestone: real OBS filter, pass-through image.
    // Processing will be connected without opening a second camera capture.
    if (!obs_source_process_filter_begin(data->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
        return;
    obs_source_t *target = obs_filter_get_target(data->context);
    const uint32_t width = target ? obs_source_get_base_width(target) : 0;
    const uint32_t height = target ? obs_source_get_base_height(target) : 0;
    obs_source_process_filter_end(data->context, data->effect ? data->effect : effect, width, height);
}
} // namespace

bool obs_module_load(void)
{
    obs_source_info filter{};
    filter.id = source_id;
    filter.type = OBS_SOURCE_TYPE_FILTER;
    filter.output_flags = OBS_SOURCE_VIDEO;
    filter.get_name = [](void *) { return "Fora de Cena"; };
    filter.create = filter_create;
    filter.destroy = filter_destroy;
    filter.update = filter_update;
    filter.get_defaults = filter_defaults;
    filter.get_properties = filter_properties;
    filter.video_render = filter_video_render;
    obs_register_source(&filter);
    blog(LOG_INFO, "[Fora de Cena] Filtro OBS carregado (protótipo).");
    return true;
}

void obs_module_unload(void) { blog(LOG_INFO, "[Fora de Cena] Filtro OBS descarregado."); }
const char *obs_module_description(void) { return "Filtro de webcam do Fora de Cena para OBS Studio."; }
