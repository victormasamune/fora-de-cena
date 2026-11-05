#include <obs-module.h>
#include <obs.h>
#include "ipc-bridge.hpp"
#include "native-planes.hpp"
#include <atomic>
#include <memory>

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
    static_cast<FilterData *>(opaque)->enabled = obs_data_get_bool(settings, "enabled");
}
void filter_defaults(obs_data_t *settings) { obs_data_set_default_bool(settings, "enabled", true); }
obs_properties_t *filter_properties(void *)
{
    auto *properties = obs_properties_create();
    obs_properties_add_bool(properties, "enabled", "Ativar filtro");
    obs_properties_add_text(properties, "development_state", "Protótipo: filtro em modo de passagem.", OBS_TEXT_INFO);
    return properties;
}
void filter_video_render(void *opaque, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    auto *data = static_cast<FilterData *>(opaque);
    if (!data || !data->context) return;
    auto *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    if (!data->enabled || !default_effect) {
        obs_source_skip_video_filter(data->context);
        return;
    }
    if (!obs_source_process_filter_begin(data->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) return;
    obs_source_process_filter_end(data->context, default_effect, 0, 0);
}

// Separate source ID preserves existing synchronous filters and saved scenes.
struct IpcFilterData {
    std::atomic<bool> enabled{false};
    fdc::Bridge bridge;
};
void ipc_update(void *opaque, obs_data_t *settings)
{
    auto *data = static_cast<IpcFilterData *>(opaque);
    bool enabled = obs_data_get_bool(settings, "enabled");
    auto port = obs_data_get_int(settings, "ipc_port");
    data->bridge.configure(enabled, port > 0 && port <= 65535 ? static_cast<uint16_t>(port) : 0,
                           obs_data_get_string(settings, "ipc_token"));
    data->enabled.store(enabled);
}
void *ipc_create(obs_data_t *settings, obs_source_t *)
{
    try {
        auto data = std::make_unique<IpcFilterData>();
        ipc_update(data.get(), settings);
        return data.release();
    } catch (const std::exception &error) {
        blog(LOG_ERROR, "[Fora de Cena IPC] %s", error.what());
        return nullptr;
    }
}
void ipc_destroy(void *opaque) { delete static_cast<IpcFilterData *>(opaque); }
void ipc_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "enabled", false);
    obs_data_set_default_int(settings, "ipc_port", 0);
    obs_data_set_default_string(settings, "ipc_token", "");
}
obs_properties_t *ipc_properties(void *opaque)
{
    auto *properties = obs_properties_create();
    obs_properties_add_text(properties, "ipc_description",
        "Teste de envio e retorno de vídeo, sem efeitos. Inicie o worker Python e copie a porta e a chave. "
        "Use em uma fonte Dispositivo de captura de vídeo. Formatos de 8 bits: BGRA/BGRX/RGBA, NV12/I420 e YUY2/UYVY/YVYU; até 3840x2160. "
        "Formato não suportado ou worker indisponível: vídeo original.", OBS_TEXT_INFO);
    obs_properties_add_int(properties, "ipc_port", "Porta exibida pelo worker", 0, 65535, 1);
    obs_properties_add_text(properties, "ipc_token", "Chave exibida pelo worker", OBS_TEXT_PASSWORD);
    obs_properties_add_bool(properties, "enabled", "Ativar teste IPC");
    auto status = opaque ? static_cast<IpcFilterData *>(opaque)->bridge.status() : "IPC desativado";
    obs_properties_add_text(properties, "ipc_status", status.c_str(), OBS_TEXT_INFO);
    return properties;
}

uint16_t pixel_format(video_format format)
{
    switch (format) {
    case VIDEO_FORMAT_BGRA: return 1;
    case VIDEO_FORMAT_BGRX: return 2;
    case VIDEO_FORMAT_RGBA: return 3;
    case VIDEO_FORMAT_I420: return 4;
    case VIDEO_FORMAT_NV12: return 5;
    case VIDEO_FORMAT_YUY2: return 6;
    case VIDEO_FORMAT_UYVY: return 7;
    case VIDEO_FORMAT_YVYU: return 8;
    default: return 0;
    }
}
std::array<fdc::Plane, 3> frame_planes(obs_source_frame *frame, uint16_t format)
{
    size_t width = frame->width, height = frame->height;
    std::array<fdc::Plane, 3> planes{};
    if (format <= 3) {
        planes[0] = {frame->data[0], frame->linesize[0], width * 4, height};
    } else if (format == 4) {
        planes[0] = {frame->data[0], frame->linesize[0], width, height};
        planes[1] = {frame->data[1], frame->linesize[1], width / 2, height / 2};
        planes[2] = {frame->data[2], frame->linesize[2], width / 2, height / 2};
    } else if (format == 5) {
        planes[0] = {frame->data[0], frame->linesize[0], width, height};
        planes[1] = {frame->data[1], frame->linesize[1], width, height / 2};
    } else {
        planes[0] = {frame->data[0], frame->linesize[0], width * 2, height};
    }
    return planes;
}
obs_source_frame *ipc_video(void *opaque, obs_source_frame *frame)
{
    auto *data = static_cast<IpcFilterData *>(opaque);
    if (!frame || !data->enabled.load()) return frame;
    const uint16_t format = pixel_format(frame->format);
    if (!format) return frame;
    try {
        const auto size = fdc::payload_size(format, frame->width, frame->height);
        const auto planes = frame_planes(frame, format);
        fdc::Frame request;
        request.format = format; request.width = frame->width; request.height = frame->height;
        request.timestamp = frame->timestamp;
        request.pixels = fdc::pack_planes(planes, size);
        // Copy original pixels before applying any reply; never feed returned video back.
        data->bridge.submit(std::move(request));
        auto reply = data->bridge.take(format, frame->width, frame->height);
        if (reply) fdc::unpack_planes(reply->pixels, planes);
        // Retain current capture timestamp/metadata. Replies older than 250ms are discarded.
    } catch (const std::exception &) {
        // Bad dimensions, stride, unsupported format or allocation failure: pass through.
    }
    return frame;
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

    obs_source_info ipc{};
    ipc.id = "fora_de_cena_ipc_filter";
    ipc.type = OBS_SOURCE_TYPE_FILTER;
    ipc.output_flags = OBS_SOURCE_ASYNC_VIDEO;
    ipc.get_name = [](void *) { return "Fora de Cena IPC (experimental)"; };
    ipc.create = ipc_create;
    ipc.destroy = ipc_destroy;
    ipc.update = ipc_update;
    ipc.get_defaults = ipc_defaults;
    ipc.get_properties = ipc_properties;
    ipc.filter_video = ipc_video;
    obs_register_source(&ipc);
    blog(LOG_INFO, "[Fora de Cena] Filtros de passagem e IPC experimental registrados.");
    return true;
}
void obs_module_unload(void) { blog(LOG_INFO, "[Fora de Cena] Filtro OBS descarregado."); }
const char *obs_module_description(void) { return "Filtro de webcam do Fora de Cena para OBS Studio."; }
