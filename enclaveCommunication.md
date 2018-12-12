# Enclave Communication in Praesidio

Application and enclave should be able to use whatever protocol they like to communicate with each other. The way that I envision communication between enclaves is that there are two one-way chunks of shared memory. Each one-way chunk of memory will have the source enclave with read and write access, while the destination enclave only has read access.

I would like two separate one-way channels as opposed to one two-way channel, because of simplicity in programming and resistance against attacks. Two channels are simpler as a model, because data transfer between domains will likely happen in the last layer cache. If two enclaves could write to the same space at the same time, then coherency will need to be ensured and could introduce side-channel attacks. Additionally, if only one enclave has write access to a piece of memory it can easily check the integrity of what it has sent.

The way that this will be implemented is besides having a tag for the page owner, the page owner can attribute another enclave to have read access to a page. There will be a special instruction that can be called on the source side to set read access to one of its pages. For now only one page per receiver will be allowed. There will then also be an instruction for the receiving side that will request the start address of the page that has been shared with them by a particular enclave.

The reason that shared memory is chosen instead of having a hardware enforced one-way FIFO is:
* Hardware simplicity: we don't need to manage the FIFO. The sending enclave is in complete control over the communication channel and manages things as they see fit.
* Communication flexibility: enclaves are now free to choose however they want to communicate. For example, protocol buffers, simple shared memory. Also, this system does not require the receiving enclave to copy data from the communication memory to private memory if it does not wish to do so.
