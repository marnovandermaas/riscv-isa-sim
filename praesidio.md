# Praesidio
> Author: Marno van der Maas

Praesidio is an extension of the Spike instruction set simulator for physically isolated enclave processor.

The idea is that enclave threads and normal threads run on separate processors. Now, Spike doesn't have a notion of a physical processor or core. It just looks at hardware threads. However, the cache simulation is important in the sense that enclave memory should be isolated from normal memory and from the memory of other enclaves.

```
|                               L2 Cache                                |
|                RMT                |   RMT  |   RMT  |   RMT  |   RMT  |
| Thread | Thread | Thread | Thread | Thread | Thread | Thread | Thread |
|           Normal World            |           Enclave World           |
```
