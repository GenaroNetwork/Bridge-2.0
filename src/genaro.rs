//use web3::futures::Future;
use web3::Transport;
use web3::types::{H256, BlockNumber, H160};
use web3::helpers::{CallFuture, serialize};
use serde::Deserialize;

// `Genaro` namespace
#[derive(Debug, Clone)]
pub struct Genaro<T> {
    transport: T,
}

impl<T: Transport> Genaro<T> {
    pub fn new(transport: T) -> Self {
        Genaro { transport }
    }
    // 获取交易信息: 购买空间
    pub fn get_bucket_tx_info(
        &self,
        from_block: Option<BlockNumber>,
        to_block: Option<BlockNumber>,
    )
        -> web3::helpers::CallFuture<Option<Vec<BucketTxInfo>>, T::Out>
    {
        let from = serialize(&from_block.unwrap_or(BlockNumber::Earliest));
        let to = serialize(&to_block.unwrap_or(BlockNumber::Latest));
        CallFuture::new(self.transport.execute("eth_getBucketTxInfo", vec![from, to]))
    }
    // 获取交易信息: 购买流量
    pub fn get_traffic_tx_info(
        &self,
        from_block: Option<BlockNumber>,
        to_block: Option<BlockNumber>,
    )
        -> web3::helpers::CallFuture<Option<Vec<TrafficTxInfo>>, T::Out>
    {
        let from = serialize(&from_block.unwrap_or(BlockNumber::Earliest));
        let to = serialize(&to_block.unwrap_or(BlockNumber::Latest));
        CallFuture::new(self.transport.execute("eth_getTrafficTxInfo", vec![from, to]))
    }
    // 空间续费
    pub fn get_bucket_supplement_tx(
        &self,
        from_block: Option<BlockNumber>,
        to_block: Option<BlockNumber>,
    )
        -> web3::helpers::CallFuture<Option<Vec<BucketSupplementTx>>, T::Out>
    {
        let from = serialize(&from_block.unwrap_or(BlockNumber::Earliest));
        let to = serialize(&to_block.unwrap_or(BlockNumber::Latest));
        CallFuture::new(self.transport.execute("eth_getBucketSupplementTx", vec![from, to]))
    }
    pub fn get_address_by_node(
        &self,
        node: H160,
    )
        -> web3::helpers::CallFuture<Option<H160>, T::Out>
    {
        let node_str = hex::encode(node.as_bytes());
        let node_value = serde_json::to_value(&node_str).unwrap();
        CallFuture::new(self.transport.execute("eth_getAddressByNode", vec![node_value]))
    }
}

#[derive(Debug, Deserialize)]
pub struct BucketTxInfo {
    address: H160,
    #[serde(rename(deserialize = "bucketId"))]
    bucket_id: String,
    #[serde(rename(deserialize = "timeStart"))]
    time_start: u32,
    #[serde(rename(deserialize = "timeEnd"))]
    time_end: u32,
    backup: u32,
    size: u32,
    hash: H256,
}

#[derive(Debug, Deserialize)]
pub struct TrafficTxInfo {
    address: H160,
    traffic: u32,
    hash: H256,
}

#[derive(Debug, Deserialize)]
pub struct BucketSupplementTx {
    address: H160,
    #[serde(rename(deserialize = "bucketId"))]
    bucket_id: String,
    size: u32,
    duration: u32,
    #[serde(rename(deserialize = "blockNum"))]
    block_num: u32,
    hash: H256,
}
