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

static void get_bucket_request_worker(uv_work_t *work)
{
    get_bucket_request_t *req = work->data;
    int status_code = 0;

    req->error_code = fetch_json(req->http_options, req->encrypt_options,
                                 req->options, req->method, req->path, NULL, req->body,
                                 req->auth, &req->response, &status_code);

    req->status_code = status_code;

    if (!req->response) {
        req->bucket = NULL;
        return;
    }

    struct json_object *id;
    struct json_object *name;
    struct json_object *created;
    struct json_object *bucketId;
    struct json_object *type;
    struct json_object *limitStorage;
    struct json_object *usedStorage;
    struct json_object *timeStart;
    struct json_object *timeEnd;

    json_object_object_get_ex(req->response, "id", &id);
    json_object_object_get_ex(req->response, "name", &name);
    json_object_object_get_ex(req->response, "created", &created);
    json_object_object_get_ex(req->response, "bucketId", &bucketId);
    json_object_object_get_ex(req->response, "type", &type);
    json_object_object_get_ex(req->response, "limitStorage", &limitStorage);
    json_object_object_get_ex(req->response, "usedStorage", &usedStorage);
    json_object_object_get_ex(req->response, "timeStart", &timeStart);
    json_object_object_get_ex(req->response, "timeEnd", &timeEnd);

    req->bucket = malloc(sizeof(genaro_bucket_meta_t));
    req->bucket->id = json_object_get_string(id);
    req->bucket->decrypted = false;
    req->bucket->created = json_object_get_string(created);
    req->bucket->bucketId = json_object_get_string(bucketId);
    req->bucket->type = json_object_get_int(type);
    req->bucket->name = NULL;
    req->bucket->limitStorage = json_object_get_int64(limitStorage);
    req->bucket->usedStorage = json_object_get_int64(usedStorage);
    req->bucket->timeStart = json_object_get_int64(timeStart);
    req->bucket->timeEnd = json_object_get_int64(timeEnd);

    const char *encrypted_name = json_object_get_string(name);
    if (encrypted_name) {
        char *decrypted_name = NULL;
        int error_status = decrypt_meta_hmac_sha512(encrypted_name,
                                                    req->encrypt_options->priv_key,
                                                    req->encrypt_options->key_len,
                                                    BUCKET_NAME_MAGIC,
                                                    &decrypted_name);  
        
        if (!error_status) {
            req->bucket->decrypted = true;
            req->bucket->name = decrypted_name;
        } else {
            req->bucket->decrypted = false;
            req->bucket->name = strdup(encrypted_name);
        }
    }
}

static void rename_bucket_request_worker(uv_work_t *work)
{
    rename_bucket_request_t *req = work->data;
    int status_code = 0;
    
    req->error_code = fetch_json(req->http_options, req->encrypt_options,
                                 req->options, req->method, req->path, NULL, req->body,
                                 req->auth, &req->response, &status_code);
    
    req->status_code = status_code;
}

static void list_files_request_worker(uv_work_t *work)
{
    list_files_request_t *req = work->data;
    int status_code = 0;

    req->error_code = fetch_json(req->http_options, req->encrypt_options,
                                 req->options, req->method, req->path, NULL, req->body,
                                 req->auth, &req->response, &status_code);

    req->status_code = status_code;

    int num_files = 0;
    if (req->response != NULL &&
        json_object_is_type(req->response, json_type_array)) {
        num_files = json_object_array_length(req->response);
    }
    
    struct json_object *file;
    struct json_object *filename;
    struct json_object *mimetype;
    struct json_object *size;
    struct json_object *id;
    struct json_object *created;
    struct json_object *isShareFile;
    struct json_object *rsaKey;
    struct json_object *rsaCtr;

    bool *p_is_share = NULL;
    if (num_files > 0) {
        p_is_share = (bool *)malloc(sizeof(bool) * num_files);
    }

    int num_visible_files = 0;
    for (int i = 0; i < num_files; i++) {
        file = json_object_array_get_idx(req->response, i);
        json_object_object_get_ex(file, "isShareFile", &isShareFile);

        p_is_share[i] = json_object_get_boolean(isShareFile);

        if(req->is_support_share || !p_is_share[i]) {
            num_visible_files++;
        }
    }

    if(num_visible_files > 0) {
        req->files = (genaro_file_meta_t *)malloc(sizeof(genaro_file_meta_t) * num_visible_files);
    }
    
    req->total_files = num_visible_files;

    int file_index = 0;
    for (int i = 0; i < num_files; i++) {
        file = json_object_array_get_idx(req->response, i);

        json_object_object_get_ex(file, "filename", &filename);
        json_object_object_get_ex(file, "mimetype", &mimetype);
        json_object_object_get_ex(file, "size", &size);
        json_object_object_get_ex(file, "id", &id);
        json_object_object_get_ex(file, "created", &created);
        json_object_object_get_ex(file, "rsaKey", &rsaKey);
        json_object_object_get_ex(file, "rsaCtr", &rsaCtr);

        // if this file is a shared file but we don't support share
        if(!req->is_support_share && p_is_share[i]) {
            continue;
        }

        genaro_file_meta_t *file_meta = &req->files[file_index];
        file_index++;

        file_meta->isShareFile = p_is_share[i];
        file_meta->created = json_object_get_string(created);
        file_meta->mimetype = json_object_get_string(mimetype);
        file_meta->size = json_object_get_int64(size);
        file_meta->erasure = NULL;
        file_meta->index = NULL;
        file_meta->hmac = NULL; // TODO though this value is not needed here
        file_meta->id = json_object_get_string(id);
        file_meta->decrypted = false;
        file_meta->filename = NULL;
        file_meta->rsaKey = json_object_get_string(rsaKey);
        file_meta->rsaCtr = json_object_get_string(rsaCtr);

        const char *encrypted_file_name = json_object_get_string(filename);
        if (!encrypted_file_name) {
            continue;
        }

        char *decrypted_file_name = NULL;
        int error_status = decrypt_meta_hmac_sha512(encrypted_file_name,
                                                    req->encrypt_options->priv_key,
                                                    req->encrypt_options->key_len,
                                                    req->bucket_id,
                                                    &decrypted_file_name);

        if (!error_status) {
            file_meta->decrypted = true;
            file_meta->filename = decrypted_file_name;
        } else {
            file_meta->decrypted = false;
            file_meta->filename = strdup(encrypted_file_name);
        }
    }

    free(p_is_share);
}

static json_request_t *json_request_new(
    genaro_http_options_t *http_options,
    genaro_encrypt_options_t *encrypt_options,
    genaro_bridge_options_t *options,
    char *method,
    char *path,
    struct json_object *request_body,
    bool auth,
    void *handle)
{

    json_request_t *req = malloc(sizeof(json_request_t));
    if (!req) {
        return NULL;
    }

    req->http_options = http_options;
    req->encrypt_options = encrypt_options;
    req->options = options;
    req->method = method;
    req->path = path;
    req->auth = auth;
    req->body = request_body;
    req->response = NULL;
    req->error_code = 0;
    req->status_code = 0;
    req->handle = handle;

    return req;
}

static list_files_request_t *list_files_request_new(
    genaro_http_options_t *http_options,
    genaro_bridge_options_t *options,
    genaro_encrypt_options_t *encrypt_options,
    bool is_support_share,
    const char *bucket_id,
    char *method,
    char *path,
    struct json_object *request_body,
    bool auth,
    void *handle)
{
    list_files_request_t *req = malloc(sizeof(list_files_request_t));
    if (!req) {
        return NULL;
    }

    req->http_options = http_options;
    req->options = options;
    req->encrypt_options = encrypt_options;
    req->is_support_share = is_support_share;
    req->bucket_id = bucket_id;
    req->method = method;
    req->path = path;
    req->auth = auth;
    req->body = request_body;
    req->response = NULL;
    req->files = NULL;
    req->total_files = 0;
    req->error_code = 0;
    req->status_code = 0;
    req->handle = handle;

    return req;
}

static create_bucket_request_t *create_bucket_request_new(
    genaro_http_options_t *http_options,
    genaro_bridge_options_t *bridge_options,
    genaro_encrypt_options_t *encrypt_options,
    const char *bucket_name,
    void *handle)
{
    create_bucket_request_t *req = malloc(sizeof(create_bucket_request_t));
    if (!req) {
        return NULL;
    }

    req->http_options = http_options;
    req->encrypt_options = encrypt_options;
    req->bridge_options = bridge_options;
    req->bucket_name = bucket_name;
    req->encrypted_bucket_name = NULL;
    req->response = NULL;
    req->bucket = NULL;
    req->error_code = 0;
    req->status_code = 0;
    req->handle = handle;

    return req;
}

static get_buckets_request_t *get_buckets_request_new(
    genaro_http_options_t *http_options,
    genaro_bridge_options_t *options,
    genaro_encrypt_options_t *encrypt_options,
    char *method,
    char *path,
    struct json_object *request_body,
    bool auth,
    void *handle)
{
    get_buckets_request_t *req = malloc(sizeof(get_buckets_request_t));
    if (!req) {
        return NULL;
    }

    req->http_options = http_options;
    req->options = options;
    req->encrypt_options = encrypt_options;
    req->method = method;
    req->path = path;
    req->auth = auth;
    req->body = request_body;
    req->response = NULL;
    req->buckets = NULL;
    req->total_buckets = 0;
    req->error_code = 0;
    req->status_code = 0;
    req->handle = handle;

    return req;
}

static get_bucket_request_t *get_bucket_request_new(
        genaro_http_options_t *http_options,
        genaro_bridge_options_t *options,
        genaro_encrypt_options_t *encrypt_options,
        char *method,
        char *path,
        struct json_object *request_body,
        bool auth,
        void *handle)
{
    get_bucket_request_t *req = malloc(sizeof(get_bucket_request_t));
    if (!req) {
        return NULL;
    }

    req->http_options = http_options;
    req->options = options;
    req->encrypt_options = encrypt_options;
    req->method = method;
    req->path = path;
    req->auth = auth;
    req->body = request_body;
    req->response = NULL;
    req->bucket = NULL;
    req->error_code = 0;
    req->status_code = 0;
    req->handle = handle;

    return req;
}

static rename_bucket_request_t *rename_bucket_request_new(
                                                    genaro_http_options_t *http_options,
                                                    genaro_bridge_options_t *options,
                                                    genaro_encrypt_options_t *encrypt_options,
                                                    char *method,
                                                    char *path,
                                                    struct json_object *request_body,
                                                    bool auth,
                                                    const char *bucket_name,
                                                    const char *encrypted_bucket_name,
                                                    void *handle)
{
    rename_bucket_request_t *req = malloc(sizeof(rename_bucket_request_t));
    if (!req) {
        return NULL;
    }
    
    req->http_options = http_options;
    req->options = options;
    req->encrypt_options = encrypt_options;
    req->method = method;
    req->path = path;
    req->auth = auth;
    req->body = request_body;
    req->response = NULL;
    req->error_code = 0;
    req->status_code = 0;
    req->bucket_name = bucket_name;
    req->encrypted_bucket_name = encrypted_bucket_name;
    req->handle = handle;
    
    return req;
}
