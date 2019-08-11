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
