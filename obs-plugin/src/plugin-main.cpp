#include <obs-module.h>
#include <obs.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("fora_de_cena_obs", "pt-BR")

namespace {
constexpr const char *source_id = "fora_de_cena_webcam_filter";

struct FilterData {
    obs_source_t *context = nullptr;
    bool enabled = true;
};

void *filter_create(obs_data_t *settings, obs_source_t *context)
{
    auto *data = new FilterData();
    data->context = context;
    data->enabled = obs_data_get_bool(settings, "enabled");
    return data;
}

void filter_destroy(void *opaque) { delete static_cast<FilterData *>(opaque); }

void filter_update(void *opaque, obs_data_t *settings)
{
    auto *data = static_cast<FilterData *>(opaque);
    data->enabled = obs_data_get_bool(settings, "enabled");
}

void filter_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "enabled", true);
}

obs_properties_t *filter_properties(void *)
{
    obs_properties_t *properties = obs_properties_create();
    obs_properties_add_bool(properties, "enabled", "Ativar filtro");

    obs_properties_add_text(properties, "development_state",
        "Protótipo: o filtro está em modo de passagem. Os efeitos ainda não foram conectados.",
        OBS_TEXT_INFO);
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

    obs_source_process_filter_end(data->context, effect, 0, 0);
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
