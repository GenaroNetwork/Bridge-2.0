# libgenaro2.0设计

​	本文在libgenaro1.0基础上进行修改。

## 和Bridge的通讯协议

​	由于Bridge的重新设计，导致libgenaro和Bridge的接口可能要修改。TBD.

## 和Farmer的通讯协议

​	由于Farmer的重新设计，导致libgenaro和Farmer的接口可能要修改。TBD.

## 上传接口

- libgenaro1.0:

上传接口：`storeFile(bucketId, fileOrData, isFilePath, options)`

其中options为dictionary，其key包括filename, progressCallback, finishedCallback, index, key, ctr, rsaKey, rsaCtr

- libgenaro2.0:

上传接口：`storeFile(bucketId, fileOrData, isFilePath, options)`

在options中增加名为isEncrypt的key，表示上传前是否对文件加密，false表示不加密，此时，index, key, ctr, rsaKey, rsaCtr字段无效。

并增加判断isEncrypt的逻辑。如果isEncrypt为false，判断是否满足快速上传的条件，如果满足直接快速上传，否则不进行加密处理。

## 快速上传

​	建议客户端在调用storeFile之前从服务器检索网络内是否有相同哈希的文件，如果存在则无需调用storeFile。

## 下载接口

​	下载接口不变，但是需要判断文件是否经过加密，如果没有加密，则无需解密。

## 增加每个Shard的多线程传输

1. 分片(Shard)策略保持不变。

2. 增加对同一个Shard进行分块(Block)的策略，对每个Block分别创建一个线程进行传输（下载的话通过设置HTTP请求头的Range字段实现，上传）。

3. 由于多个Shards是并行传输的，如果增加了多个Blocks的并行传输，将导致线程数很多，所以修改并行下载Shards的最大个数GENARO_DOWNLOAD_CONCURRENCY的值为10（原来为24）。1.0版本没有对上传的Shards最大个数进行限制，需要考虑增加限制。

### 分块策略

​	由于Shards的最大下载并发数为10，所以如果分块数量太多，建立太多的线程，会导致内存占用太高。由于Shard的大小`shard_size`的范围为(0, 4GB]，分块策略设计如下（其中，`block_num`表示块数）：

- 如果`shard_size` <= 4M, 那么`block_num` = 1，即不分块；

- 如果`shard_size` > 4M 且 <= 8M，那么`block_num` = 2；

- 如果`shard_size` > 8M 且 <= 12M，那么`block_num` = 3；

- 如果`shard_size` > 12M 且 <= 16M，那么`block_num` = 4；

- 如果`shard_size` > 16M 且 <= 512M，那么`block_num` = 5；

- 如果`shard_size` > 512M 且 <= 1G，那么`block_num` = 6；

- 如果`shard_size` > 1G 且 <= 4G，那么`block_num` = 7。

前`block_num-1`个block的大小`block_size` = `ceil(shard_size / block_num)`，最后1个block大小`last_block_size` = `shard_size` - `block_size` * `block_num`。

PS：存在的问题：假设同时下载10个100M的文件，根据libgenaro1.0的分片策略，共分成22片，同时下载10片，每片的block_num为5，也就是可能同时有10 * 10 * 5 = 500个线程在运行，线程调度的开销非常大，并且由于每个线程一般至少占8MB内存，单纯下载的线程可能就会占4GB左右虚存。

## 断点续传

​	对于暂时性的断网，或者暂停后的断点续传相对简单，如果要考虑关机后下次继续上传或下载，对于上传来说可能会有点问题，因为每次上传都会重新选择Farmer。对于下载很好实现，因为下载的数据保存在本地，即使重新分配了Farmer，也可以继续从其他Farmer中下载未下载完的数据。

​	考虑到token的时效性，以及farmer可能经常变化，此次**只考虑短暂性的暂停**，即如果用户不退出程序，将可以断点续传，一旦退出，将无法断点续传。

### 下载

​	增加如下接口：

- int genaro_bridge_resolve_file_pause(genaro_download_state_t *state)

  暂停下载任务。

- int genaro_bridge_resolve_file_resume(genaro_download_state_t *state)

  恢复下载任务。当用户暂停下载任务后，可以恢复下载；当下载任务下载失败后，也可以根据具体情况判断是否能恢复下载，不能的话就重新下载。恢复下载时不会下载已经完成的内容，即进行断点续传。
  

### 上传

​	增加如下接口：

- int genaro_bridge_store_file_pause(genaro_upload_state_t *state)

  暂停上传任务。

- int genaro_bridge_store_file_resume(genaro_upload_state_t *state)

  恢复上传任务。当用户暂停上传任务后，可以恢复上传；当上传任务上传失败后，也可以根据具体情况判断是否能恢复上传，不能的话就重新上传。恢复上传时不会上传已经完成的内容，即进行断点续传。`	