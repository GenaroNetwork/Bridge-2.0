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
	