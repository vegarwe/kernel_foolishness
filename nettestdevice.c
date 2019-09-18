//Network basic driver with PING implementation

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/tcp.h>
#include <net/checksum.h>

/*

/etc/hosts:
   192.168.0.1     network1-host1
   192.168.0.2     network1-host2
/etc/networks:
   network1 192.168.0.0

   sudo ifconfig interface1 192.168.0.1

***********************TCP Packet*************************

Ethernet Header
   |-Destination Address : 00-25-5E-1A-3D-F1 
   |-Source Address      : 00-1C-C0-F8-79-EE 
   |-Protocol            : 8 

IP Header
   |-IP Version        : 4
   |-IP Header Length  : 5 DWORDS or 20 Bytes
   |-Type Of Service   : 0
   |-IP Total Length   : 141  Bytes(Size of Packet)
   |-Identification    : 13122
   |-TTL      : 64
   |-Protocol : 6
   |-Checksum : 45952
   |-Source IP        : 192.168.1.6
   |-Destination IP   : 74.125.71.125

TCP Header
   |-Source Port      : 33655
   |-Destination Port : 5222
   |-Sequence Number    : 78458457
   |-Acknowledge Number : 2427066746
   |-Header Length      : 5 DWORDS or 20 BYTES
   |-Urgent Flag          : 0
   |-Acknowledgement Flag : 1
   |-Push Flag            : 1
   |-Reset Flag           : 0
   |-Synchronise Flag     : 0
   |-Finish Flag          : 0
   |-Window         : 62920
   |-Checksum       : 21544
   |-Urgent Pointer : 0

                        DATA Dump                         
IP Header
    00 25 5E 1A 3D F1 00 1C C0 F8 79 EE 08 00 45 00         .%^.=.....y...E.
    00 8D 33 42                                             ..3B
TCP Header
    40 00 40 06 B3 80 C0 A8 01 06 4A 7D 47 7D 83 77         @.@..?....J}G}.w
    14 66 04 AD                                             .f..
Data Payload
    17 03 01 00 60 A0 9C 5D 14 A1 25 AB CE 8B 7C EB         ....`..]..%...|.
    1A A4 43 A6 60 DD E8 6B 6E 43 C1 94 6A D2 25 23         ..C.`..knC..j.%#
    03 98 59 67 1A 2C 07 D3 7E B2 B8 9F 83 38 4C 69         ..Yg.,..~....8Li
    D3 3A 8E 0D 9E F0 6B CE 9E 6B F4 E1 BD 9E 50 53         .:....k..k....PS
    6D F6 AB 11 05 D6 41 82 F0 03 0C A6 E2 48 2B 71         m.....A......H+q
    16 81 FF 5B DF 50 D4 5B AD 90 04 5E 4C 94 E7 9B         ...[.P.[...^L...
    0B 72 7E 32 88                                          .r~2.

###########################################################

*/



struct nettestdevice_priv {
      struct net_device_stats stats;
      struct sk_buff *skb;
      struct net_device *dev;
};

static struct net_device *interface1;
static struct nettestdevice_priv *priv0;

//Method to initiate the transmission of a packet
int nettestdevice_start_xmit(struct sk_buff *skb, struct net_device *dev) 
{
	   
	char *data = skb->data;
	int len = skb->len;
	struct sk_buff *skb_priv;
	struct icmphdr* icmph;


	 struct iphdr *ih = (struct iphdr *)(data+sizeof(struct ethhdr));
	 u32 *saddr = &ih->saddr;
	 u32 *daddr = &ih->daddr;

	 printk(KERN_DEBUG"(nettestdevice) start_xmit Initial Internet address =: %d.%d.%d.%d --> %d.%d.%d.%d\n",
		    ((u8 *)saddr)[0], ((u8 *)saddr)[1], ((u8 *)saddr)[2], ((u8 *)saddr)[3],
		    ((u8 *)daddr)[0], ((u8 *)daddr)[1], ((u8 *)daddr)[2], ((u8 *)daddr)[3]);
	  
	 printk(KERN_DEBUG"(nettestdevice) ih->protocol = %d\n", (int) ih->protocol );
	 
	 
	 //Save onto private struct
	 if( priv0 )
	 {
	    priv0->skb = skb;
	    priv0->dev = dev;
	 }
	 

	
	if( ih->protocol == IPPROTO_ICMP) //IPPROTO_ICMP 1
	{
	    	printk(KERN_INFO"(nettestdevice) IPPROTO_ICMP!\n");
		icmph = icmp_hdr(skb);
	
		//#define	ICMP_ECHO 8/* echo service */
		if( icmph->type == ICMP_ECHO)  
		{
		    printk(KERN_INFO"(nettestdevice) IPPROTO_ECHO!\n");
		    
		    
		    icmph->type = ICMP_ECHOREPLY;   //#define  ICMP_ECHOREPLY	0 /* echo reply */
		    ((u8*)saddr)[3] = 2;
		    ((u8*)daddr)[3] = 1;
		 
		    printk(KERN_DEBUG"(nettestdevice) start_xmit ICMP_ECHO %d.%d.%d.%d --> %d.%d.%d.%d\n",
		      ((u8 *)saddr)[0], ((u8 *)saddr)[1], ((u8 *)saddr)[2], ((u8 *)saddr)[3],
		      ((u8 *)daddr)[0], ((u8 *)daddr)[1], ((u8 *)daddr)[2], ((u8 *)daddr)[3]);
		  
		  
		    struct ethhdr *eth = (struct ethhdr*)data;//skb_push(skb,ETH_HLEN);
		    memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);
 		    
		    skb->dev = dev;
		    skb->protocol =  eth_type_trans(skb, dev);
		    //Recompute the checksum. Verify for integrity after modification
		    ih->check  = 0;
		    ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);
		  
		    //Packet from device driver is queued for processing by upper (protocol) level.
		    netif_rx(skb);
		  
		    if( priv0 )
			  priv0->stats.tx_packets++;
		    return NETDEV_TX_OK;
	   
		}
	  }
	  
	  dev_kfree_skb(skb);
	  return 0;
}


	
struct net_device_stats *nettestdevice_stats(struct net_device *dev) 
{
	return &(((struct nettestdevice_priv*)netdev_priv(dev))->stats);
}

int nettestdevice_open(struct net_device *dev) 
{
	printk(KERN_DEBUG"(nettestdevice) nettestdevice_open()\n");

	//Kernel function to start the queue
	netif_start_queue(dev); 
	return 0; 
}

int nettestdevice_stop(struct net_device *dev) 
{
	printk(KERN_DEBUG"(nettestdevice) nettestdevice_stop()\n");

	//Stop the queue
	netif_stop_queue(dev); 
	return 0; 
}

static const struct net_device_ops nettestdevice_device_ops = 
{
      .ndo_open = nettestdevice_open,
      .ndo_stop = nettestdevice_stop,
      .ndo_start_xmit = nettestdevice_start_xmit,
      .ndo_get_stats = nettestdevice_stats,
};


int nettestdevice_header(struct sk_buff *skb, 
			 struct net_device *dev,
		    	 unsigned short type, 
			 const void *daddr, 
			 const void *saddr,
		  	 unsigned int len) 
{
	return dev->hard_header_len;

}
	
static const struct header_ops nettestdevice_header_ops = 
{
	.create  = nettestdevice_header,
};


int init_module (void) 
{
	int i;
	
	interface1 = alloc_etherdev(sizeof(struct nettestdevice_priv));
	
	for (i=0 ; i < 6 ; i++) interface1->dev_addr[i] = (unsigned char)i;
	for (i=0 ; i < 6 ; i++) interface1->broadcast[i] = (unsigned char)15;//0xF
	interface1->hard_header_len = 14;

	memcpy(interface1->name, "interface1\0", 11);
	
	interface1->netdev_ops = &nettestdevice_device_ops;
	
	//interface1->header_ops = &nettestdevice_header_ops;
	
	//No ARP
	interface1->flags |= IFF_NOARP;
	
	//Access network device private data
	priv0  = netdev_priv(interface1);
	
	//Register the devices 
	register_netdev(interface1);
	printk(KERN_DEBUG"(nettestdevice) Interfaces registered successfully.\n");

	return 0;


}

void cleanup_module(void) 
{

	if (interface1) 
   	{
	 	unregister_netdev(interface1);
	     	free_netdev(interface1);
	}
	
	printk(KERN_DEBUG"(nettestdevice) cleanup_module()\n");
}

MODULE_LICENSE("GPL");
