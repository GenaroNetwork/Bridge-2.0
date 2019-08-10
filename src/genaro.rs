//use web3::futures::Future;
use web3::Transport;
use web3::types::{BlockNumber, U256, H256, H160};
use web3::helpers::{CallFuture, serialize};
use serde::{Deserialize, Serialize};

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
    pub fn get_address_by_node<N: AsRef<str> + Serialize>(
        &self,
        node: N,
    )
        -> web3::helpers::CallFuture<Option<H160>, T::Out>
    {
        let node_value = serialize(&node);
        CallFuture::new(self.transport.execute("eth_getAddressByNode", vec![node_value]))
    }
    pub fn get_storage_nodes(
        &self,
        address: H160,
    )
        -> web3::helpers::CallFuture<Option<Vec<String>>, T::Out>
    {
        let address_value = serialize(&address);
        CallFuture::new(self.transport.execute("eth_getStorageNodes", vec![address_value]))
    }
    pub fn get_stake(
        &self,
        address: H160,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<U256>, T::Out>
    {
        let address_value = serialize(&address);
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getStake", vec![address_value, block_value]))
    }
    pub fn get_already_back_stake_list(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<Vec<U256>>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getAlreadyBackStakeList", vec![block_value]))
    }
    pub fn get_heft(
        &self,
        address: H160,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<U256>, T::Out>
    {
        let address_value = serialize(&address);
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getHeft", vec![address_value, block_value]))
    }
    pub fn get_candidates(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<Vec<H160>>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getCandidates", vec![block_value]))
    }
    pub fn get_committee_rank(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<Vec<H160>>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getCommitteeRank", vec![block_value]))
    }
    pub fn get_main_account_rank(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<Vec<H160>>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getMainAccountRank", vec![block_value]))
    }
    pub fn get_genaro_code_hash(
        &self,
        address: H160,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<String>, T::Out>
    {
        let address_value = serialize(&address);
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getGenaroCodeHash", vec![address_value, block_value]))
    }
    pub fn get_extra(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<ExtraInfo>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getExtra", vec![block_value]))
    }
    pub fn get_genaro_price(
        &self,
        block: BlockNumber,
    )
        -> web3::helpers::CallFuture<Option<GenaroPrice>, T::Out>
    {
        let block_value = serialize(&block);
        CallFuture::new(self.transport.execute("eth_getGenaroPrice", vec![block_value]))
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

#[derive(Debug, Deserialize)]
pub struct ExtraInfo {
    #[serde(rename(deserialize = "committeeRank"))]
    committee_rank: Option<String>,
    #[serde(rename(deserialize = "lastBlockNum"))]
    last_block_num: u32,
    #[serde(rename(deserialize = "lastSynBlockHash"))]
    last_syn_block_hash: H256,
    signature: String,
    ratio: Option<f64>,
    #[serde(rename(deserialize = "CommitteeAccountBinding"))]
    committee_account_binding: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct GenaroPrice {
    #[serde(rename(deserialize = "bucketPricePerGperDay"))]
    bucket_price_per_g_day: U256,
    #[serde(rename(deserialize = "oneDayMortgageGes"))]
    one_day_mortgage_ges: U256,
    #[serde(rename(deserialize = "oneDaySyncLogGsaCost"))]
    one_day_sync_log_gsa_cost: U256,
    #[serde(rename(deserialize = "stakeValuePerNode"))]
    stake_value_per_node: U256,
    #[serde(rename(deserialize = "trafficPricePerG"))]
    traffic_price_per_g: U256,
}
