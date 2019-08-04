use web3::futures::Future;
use web3::types::BlockNumber;

mod genaro;

fn main() {
    let (_eloop, ws) = web3::transports::WebSocket::new("ws://47.100.34.71:8547").unwrap();
    let web3 = web3::Web3::new(ws.clone());
    let blocks = web3.eth().block_number().wait().unwrap();
    println!("{:#?}", blocks);

    let accounts = web3.eth().accounts().wait().unwrap();
    println!("{:#?}", accounts);

    // 测试 get_bucket_tx_info
    let g = genaro::Genaro::new(ws.clone());
    let bucket_tx_info = g.get_bucket_tx_info(
        Some(BlockNumber::Number(99999)),
        Some(BlockNumber::Number(186399)))
        .wait().unwrap();
    println!("{:#?}", bucket_tx_info);

    // 测试 get_traffic_tx_info
    let traffic_tx_info = g.get_traffic_tx_info(
        Some(BlockNumber::Number(200000)),
        Some(BlockNumber::Number(286400)))
        .wait().unwrap();
    println!("{:#?}", traffic_tx_info);

    let bucket_supplement_tx = g.get_bucket_supplement_tx(
        Some(BlockNumber::Number(200000)),
        Some(BlockNumber::Number(286400)))
        .wait().unwrap();
    println!("{:#?}", bucket_supplement_tx);

    let addr = g.get_address_by_node("511cb72a70f522cc4becfb9400cecf4b1ccc2916")
        .wait().unwrap();
    println!("{:#?}", addr);

    let addr_vec = hex::decode("73e39b82d3fE58B52F718ea1aB85B4f4929e20d1").unwrap();
    let mut addr = [0u8; 20];
    addr.clone_from_slice(&addr_vec);
    let addr = g.get_storage_nodes(addr.into())
        .wait().unwrap();
    println!("{:#?}", addr);

    let addr_vec = hex::decode("871860e8854bc539ab2127b2c91637aebab22a1f").unwrap();
    let mut addr = [0u8; 20];
    addr.clone_from_slice(&addr_vec);
    let balance = web3.eth().balance(addr.into(), None).wait().unwrap();
    println!("{:#?}", balance)
}
