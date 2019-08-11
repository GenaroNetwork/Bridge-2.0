#include "genaro.h"
#include "http.h"
#include "utils.h"
#include "crypto.h"
#include "key_file.h"

static inline void noop() {};

static const char *BUCKET_OP[] = { "PUSH", "PULL" };

/*the value of ENV:GENARO_DEBUG, the debug level(4:debug, 3:info, 2:warn, 1:error), 
it's prior to log_level of log_options*/
int genaro_debug = 0;

/*Curl info output directory, used only for debug*/
char *curl_out_dir = NULL;

key_result_t *genaro_parse_key_file(json_object *key_json_obj, const char *passphrase)
{
    key_file_obj_t *key_file_obj = get_key_obj(key_json_obj);
    if (key_file_obj == KEY_FILE_ERR_POINTER)
    {
        goto parse_fail;
    }
    key_result_t *key_result = NULL;
    int status = extract_key_file_obj(passphrase, key_file_obj, &key_result);
    if (status != KEY_FILE_SUCCESS)
    {
        goto parse_fail;
    }
    return key_result;

parse_fail:
    if (key_file_obj)
    {
        key_file_obj_put(key_file_obj);
    }
    return NULL;
}

/**
 * pass keys from key_result to encrypt_options
 * @param[in] key_result will be freed
 * @param[in] encrypt_options
 */
void genaro_key_result_to_encrypt_options(key_result_t *key_result, genaro_encrypt_options_t *encrypt_options)
{
    encrypt_options->priv_key = key_result->priv_key;
    encrypt_options->key_len = key_result->key_len;
    free(key_result->dec_key);
    free(key_result);
}

static uv_work_t *uv_work_new()
{
    uv_work_t *work = malloc(sizeof(uv_work_t));
    return work;
}

static void json_request_worker(uv_work_t *work)
{
    json_request_t *req = work->data;
    int status_code = 0;

    req->error_code = fetch_json(req->http_options, req->encrypt_options,
                                 req->options, req->method, req->path, NULL, req->body,
                                 req->auth, &req->response, &status_code);

    req->status_code = status_code;
}

static void get_buckets_request_worker(uv_work_t *work)
{
    get_buckets_request_t *req = work->data;
    int status_code = 0;

    req->error_code = fetch_json(req->http_options, req->encrypt_options,
                                 req->options, req->method, req->path, NULL, req->body,
                                 req->auth, &req->response, &status_code);

    req->status_code = status_code;

    int num_buckets = 0;
    if (req->response != NULL &&
        json_object_is_type(req->response, json_type_array)) {
        num_buckets = json_object_array_length(req->response);
    }

    if (num_buckets > 0) {
        req->buckets = calloc(num_buckets, sizeof(genaro_bucket_meta_t));
        req->total_buckets = num_buckets;
    }

    struct json_object *bucket_item;
    struct json_object *id;
    struct json_object *name;
    struct json_object *created;
    struct json_object *bucketId;
    struct json_object *type;
    struct json_object *limitStorage;
    struct json_object *usedStorage;
    struct json_object *timeStart;
    struct json_object *timeEnd;

    for (int i = 0; i < num_buckets; i++) {
        bucket_item = json_object_array_get_idx(req->response, i);

        json_object_object_get_ex(bucket_item, "id", &id);
        json_object_object_get_ex(bucket_item, "name", &name);
        json_object_object_get_ex(bucket_item, "created", &created);
        json_object_object_get_ex(bucket_item, "bucketId", &bucketId);
        json_object_object_get_ex(bucket_item, "type", &type);
        json_object_object_get_ex(bucket_item, "limitStorage", &limitStorage);
        json_object_object_get_ex(bucket_item, "usedStorage", &usedStorage);
        json_object_object_get_ex(bucket_item, "timeStart", &timeStart);
        json_object_object_get_ex(bucket_item, "timeEnd", &timeEnd);

        genaro_bucket_meta_t *bucket = &req->buckets[i];
        bucket->id = json_object_get_string(id);
        bucket->decrypted = false;
        bucket->created = json_object_get_string(created);
        bucket->bucketId = json_object_get_string(bucketId);
        bucket->type = json_object_get_int(type);
        bucket->name = NULL;
        bucket->limitStorage = json_object_get_int64(limitStorage);
        bucket->usedStorage = json_object_get_int64(usedStorage);
        bucket->timeStart = json_object_get_int64(timeStart);
        bucket->timeEnd = json_object_get_int64(timeEnd);

        const char *encrypted_name = json_object_get_string(name);
        if (!encrypted_name) {
            continue;
        }

        char *decrypted_name = NULL;
        int error_status = decrypt_meta_hmac_sha512(encrypted_name,
                                                    req->encrypt_options->priv_key,
                                                    req->encrypt_options->key_len,
                                                    BUCKET_NAME_MAGIC,
                                                    &decrypted_name);

        if (!error_status) {
            bucket->decrypted = true;
            bucket->name = decrypted_name;
        } else {
            bucket->decrypted = false;
            bucket->name = strdup(encrypted_name);
        }
    }
}
