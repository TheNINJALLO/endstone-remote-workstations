#ifndef ONI_VCF_ABI_H
#define ONI_VCF_ABI_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define VCF_CALL __cdecl
#ifdef VCF_BUILD
#define VCF_EXPORT __declspec(dllexport)
#else
#define VCF_EXPORT
#endif
#else
#define VCF_CALL
#define VCF_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define VCF_ABI_VERSION_1_0 0x00010000u
#define VCF_ABI_VERSION 0x00010001u
typedef uint64_t vcf_handle;
typedef int32_t vcf_status;
enum { VCF_OK=0, VCF_INVALID=1, VCF_VERSION=2, VCF_NOT_FOUND=3, VCF_DENIED=4,
 VCF_UNAVAILABLE=5, VCF_STALE=6, VCF_CONFLICT=7, VCF_CAPACITY=8, VCF_WRONG_THREAD=9,
 VCF_REENTRANT=10, VCF_QUARANTINED=11, VCF_INTERNAL=12, VCF_CLOSED=13, VCF_BUFFER=14, VCF_PENDING=15 };
enum { VCF_REAL_SOURCE=0, VCF_TRANSIENT=1, VCF_PERSISTENT=2, VCF_NATIVE_CONTEXT=3, VCF_CUSTOM_REPLACEMENT=4 };
enum { VCF_PREPARING=0, VCF_OPENING=1, VCF_ACTIVE=2, VCF_CLOSING=3, VCF_RECOVERING=4, VCF_TERMINAL=5 };
enum { VCF_INSERT=1, VCF_EXTRACT=2, VCF_RESULT=4, VCF_PREVIEW=8 };
enum { VCF_EVENT_OPEN=1, VCF_EVENT_CLOSE=2, VCF_EVENT_ACTION=3, VCF_EVENT_FAILURE=4, VCF_EVENT_COMMIT=5 };
typedef struct { const char *data; uint32_t length; } vcf_string;
typedef struct { const uint8_t *data; uint32_t length; } vcf_bytes;
typedef struct { uint32_t size, version; vcf_string name; } vcf_consumer_desc;
/* All views are borrowed for the duration of the call. Inputs are copied.
   NBT is bounded, standard little-endian named compound NBT; never SNBT. */
typedef struct { uint32_t size, version; vcf_string identifier; uint32_t count; vcf_bytes nbt; } vcf_item;
typedef struct {
 uint32_t size, version; vcf_handle ticket; vcf_string player; uint32_t kind;
 vcf_status result; uint64_t revision; vcf_string detail;
} vcf_event;
typedef vcf_status (VCF_CALL *vcf_callback)(void *context, const vcf_event *event);
typedef struct {
 uint32_t size, version; vcf_string name, permission; uint32_t exported;
 vcf_callback callback; void *context;
} vcf_action_desc;
typedef struct {
 uint32_t size, version; vcf_string canonical_id, player, title, source_permission;
 uint32_t mode; vcf_string dimension; int32_t x,y,z; vcf_string entity_id, held_id;
 vcf_callback callback; void *context;
} vcf_session_desc;
typedef struct { uint32_t size, version; uint32_t state; vcf_status result; uint64_t revision, generation; } vcf_session_info;
typedef struct { uint32_t size, version; vcf_string label, action, icon; } vcf_button;
typedef struct {
 uint32_t size, version; vcf_string player,title,content,permission;
 const vcf_button *buttons; uint32_t button_count; vcf_callback callback; void *context;
} vcf_menu_desc;
typedef struct {
 uint32_t size, version; vcf_string id, family, permission, aliases, source_kind;
 uint32_t native_available, custom_available; vcf_status reason_code; vcf_string reason;
} vcf_capability;
/* A rule is a complete, exact-metadata transformation. Preview extraction
   cannot execute it. Selection/execution requires an admitted adapter. */
typedef struct { uint32_t size, version, slot; vcf_item item; } vcf_cost;
typedef struct {
 uint32_t size, version; vcf_string id; uint64_t revision, stock;
 const vcf_cost *costs; uint32_t cost_count, output_slot; vcf_item output;
 uint32_t duration_ticks;
} vcf_rule_desc;
typedef struct {
 uint32_t size, version; vcf_string name, consumer, permission;
 uint32_t exported; uint64_t revision;
} vcf_action_info;
enum { VCF_GUARD_DISPATCH=1, VCF_GUARD_READY=2, VCF_GUARD_ACTIVE=3 };
typedef struct {
 uint32_t size, version; vcf_handle ticket, requesting_consumer;
 uint32_t phase; vcf_session_desc request;
} vcf_guard_event;
typedef vcf_status (VCF_CALL *vcf_guard_callback)(void *,const vcf_guard_event *);
typedef struct {
 uint32_t size, version; vcf_string name, canonical_id;
 vcf_guard_callback callback; void *context;
} vcf_guard_desc;
/* C ABI allocations never cross ownership domains. Caller supplies output
   structures. Capability strings remain valid until provider shutdown.
   All calls except get_api require the provider's server thread.
   Never unload a consumer before unregister returns VCF_OK. */
typedef struct vcf_api {
 uint32_t size, version;
 vcf_status (VCF_CALL *register_consumer)(const vcf_consumer_desc *,vcf_handle *);
 vcf_status (VCF_CALL *unregister_consumer)(vcf_handle);
 vcf_status (VCF_CALL *catalog_count)(uint32_t *);
 vcf_status (VCF_CALL *capability)(uint32_t,vcf_capability *);
 vcf_status (VCF_CALL *resolve)(vcf_string,uint32_t *);
 vcf_status (VCF_CALL *register_action)(vcf_handle,const vcf_action_desc *);
 vcf_status (VCF_CALL *unregister_action)(vcf_handle,vcf_string);
 vcf_status (VCF_CALL *invoke)(vcf_handle,vcf_string,vcf_string,vcf_handle *);
 vcf_status (VCF_CALL *prepare)(vcf_handle,const vcf_session_desc *,vcf_handle *);
 vcf_status (VCF_CALL *set_item)(vcf_handle,vcf_handle,uint32_t,const vcf_item *,uint32_t);
 vcf_status (VCF_CALL *define_rule)(vcf_handle,vcf_handle,const vcf_rule_desc *);
 vcf_status (VCF_CALL *open)(vcf_handle,vcf_handle);
 vcf_status (VCF_CALL *close)(vcf_handle,vcf_handle);
 vcf_status (VCF_CALL *session_info)(vcf_handle,vcf_handle,vcf_session_info *);
 vcf_status (VCF_CALL *show_menu)(vcf_handle,const vcf_menu_desc *,vcf_handle *);
 vcf_status (VCF_CALL *forget)(vcf_handle,vcf_handle);
 /* ABI 1.1 tail. Old 1.0 consumers negotiate only the prefix above.
    Listing exposes own actions and explicit exports. action_info copies all
    strings into caller storage; VCF_BUFFER reports required bytes. Reusing an
    outdated registry revision returns VCF_STALE without a partial result. */
 vcf_status (VCF_CALL *action_count)(vcf_handle,uint32_t *,uint64_t *);
 vcf_status (VCF_CALL *action_info)(vcf_handle,uint32_t,uint64_t,vcf_action_info *,char *,uint32_t,uint32_t *);
 vcf_status (VCF_CALL *register_guard)(vcf_handle,const vcf_guard_desc *,vcf_handle *);
 vcf_status (VCF_CALL *unregister_guard)(vcf_handle,vcf_handle);
} vcf_api;
#define VCF_API_1_0_SIZE ((uint32_t)offsetof(vcf_api,action_count))
VCF_EXPORT vcf_status VCF_CALL oni_vcf_get_api(uint32_t version, uint32_t size, vcf_api *output);
#ifdef __cplusplus
}
#endif
#endif
