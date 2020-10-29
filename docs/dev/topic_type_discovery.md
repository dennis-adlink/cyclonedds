# Topic Discovery

Topic discovery enables the exchange of topic information between endpoints using the built-in topic announcer and detector endpoints as described in the DDS RTPS Specification section 8.5. Topic discovery is based on the SEDP protocol (section 8.4.5 of the spec).

This document describes the types (entities) used in topic discovery in the DDSC and DDSI layers. Next the implementation of topic discovery using the DDS SEPD protocol is described, and after that the API functions and built-in writers that can be used by applications.

## DDSC types

In the DDSC layer, the following types are related to topic discovery:

### dds_topic

A *dds_topic* has a counted reference to a *dds_ktopic*, which stores the type name and QoS for a topic. Topics that share the same type name and QoS have a reference to the same dds_ktopic. A topic has a reference to the DDSI *sertype* that is associated with the topic.

### dds_ktopic

A *dds_ktopic* maps the topic name to its corresponding type name and QoS, and contains a mapping to the associated DDSI topic entities. The mapping to DDSI topics uses the type identifier (of the sertype associated with the dds_topic) as the key and stores a pointer to the DDSI topic entity and the (DDSI) GUID of this entity.

dds_ktopics are scoped to a participant: a dds_participant has an AVL tree to keep track of the ktopics

## DDSI types

This section describes the DDSI types that are used to represent local and discovered topics.

## topic_definition

The topic discovery implementation introduces the *topic_definition* type. A topic definition has a key, which is a hash value calculated from its type identifier and QoS (the latter also contains the topic name and type name for the topic). It also contains a reference to a DDSI sertype, a QoS struct and a type identifier. Topic definitions are stored in a domain scoped hash table (in the domain global variables).

### topic

A DDSI *topic* consists of a reference to a topic definition and a reference (back reference) to the participant. A DDSI topic is an entity (entity kind `EK_TOPIC`): it has an entity_common element and is stored in the (domain scoped) entity index. As the DDS spec does not provide an entity kind for DDSI topic entities, vendor specific entity kinds `NN_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN` and `NN_ENTITYID_KIND_CYCLONE_TOPIC_USER` are used.

Multiple dds_topics can share a single DDSI topic entity. First these topics have to share their dds_ktopic, so they have the same type name and the same QoS. In case the type identifier of their types is also equal, the topics will get a reference to the same DDSI topic.

### proxy_topic

Topics that are discovered via received SEDP messages are stored as DDSI proxy topics. A proxy topic is not a DDSI entity: proxy topics are stored in a list in the proxy participant they belong to. A proxy topic object has an entity ID field that contains the entity ID part of its GUID (the prefix for the GUID is equal to the prefix of the participants GUID). A proxy topic also has a reference to a topic definition, which can be shared with other (proxy) topics.

Proxy topic GUIDs are added in the proxy GUID list in the type lookup meta-data records, to keep track of the proxy topics that are referring to the type lookup meta-data.

## SEDP for topic discovery

Topic discovery in Cyclone is implemented by SEDP writers and readers that exchange the topic information. The key for the SEDP samples is the topic definition hash that is described above. This key is sent in a vendor specific plist parameter `PID_CYCLONE_TOPIC_GUID` (vendor specific parameter 0x1b). The endpoint GUID parameter (as used for endpoint discovery) is not used here, because the value is a (16 bytes) hash value and not a GUID.

Introducing a new DDSI topic triggers writing a SEDP (topic) sample. As DDSI topics can be shared over multiple dds_topics, this does not necessarily mean that for every dds_topic that is created in a node an SEDP sample will be written. E.g. when introducing a new dds_topic by calling the dds_find_topic function, this topic will be re-using an existing DDSI topic (as the topic is already known in the node).
The built-in endpoints for exchanging topic discovery information can be enabled/disabled via the configuration option `//CycloneDDS/Domain/Discovery/EnableTopicDiscoveryEndpoints`.

### The built-in DCPSTopic topic

Topic discovery uses a local built-in (orphan) writer that writes a DCPSTopic sample when a new topic is discovered. New topic in this context means that the topic definition for the topic that was created locally or discovered by SEDP was not yet in the topic definition administration, i.e. has a key that was not found in the global topic definition hash table. Subsequent topics (local or discovered) that are using the same topic definition are not reported by the built-in topics writer (possibly a new built-in writer that reports all topic instances may be added in a future release of Cyclone).

Similar to the built-in publication and subscription writers, the topics writer is optimized so that it only creates and returns a sample when a reader requests data. The writer has a custom writer history cache implementation that enumerates the current set of topic definitions (for all participants) at the moment data is requested (i.e. a reader reads the data). This reduces the overhead of topic discovery significantly when there are not readers.

### Find topic API

The API for finding a topic is split into two functions: `dds_find_topic_locally` and `dds_find_topic_globally`. The former can be used to search for a topic that was locally created and the latter will search for topics that are discovered using the built-in SEDP endpoint, as well as for locally created topics. Both functions take an optional time-out parameter (can be 0 to disable) to allow waiting for the topic to become available in case no topic is found at the moment of calling the find function.

The time-out mechanism is implemented using a condition variable that is triggered when new topic definitions are registered. When triggered, the find topic implementation will search again in the updated set of topic definitions.

When a topic definition is found, the find topic implementation will look for the sertype. In case a local topic is found, the sertype from that topic is used to create the topic that will be returned from the find function call. In case a remote (discovered) topic is found, the sertype may not be known. In that case, `dds_domain_resolve_type` will be used to retrieve the type.

The dds_find_topic functions require a dds_entity as first argument. This can be either a dds_domain or a dds_participant. In case a domain is provided, the topic that is found is created on the first (in the entity index) participant for this domain.

# Type Discovery

Type discovery in Cyclone is based on the DDS-XTypes type discovery, as described in section 7.6.3 of the DDS-XTypes specification.
Note: as Cyclone currently (end 2020) does not yet support the XTypes type system, the implementation of type discovery is based on the existing type system in Cyclone and is not interoperable with other vendors.

## Type Identifiers

The type identifier for a sertype is retrieved by using the typeid_hash function that is part of the ddsi_sertype_ops interface. Currently this hash function is only implemented for `ddsi_sertype_default`, as the other implementations in Cyclone (*sertype_builtin_topic*, *sertype_plist* and *sertype_pserop*) are not used for application types and not retrieved using type discovery.

The ddsi_sertype_default implementation of the sertype interface uses a MD5 hash of the (little endian) serialized sertype as the type identifier. This simplified approach (compared to TypeIdentifier and TypeObject definition in the XTypes specification) does not allow to include full type information in the hash, and it also does not allow assignability checking for two type identifiers (see below).
The implementation of type discovery adds a type identifier field to the (proxy) endpoints in DDSI that stores the identifier of the type used by the endpoint. This field is e.g. used when creating discovery data for an entity (see below).

## Type resolving
Discovery information (SEDP messages) for topics, publications, and subscriptions contains the type identifier; not the full type information. This allows remote nodes to identify the type used by a proxy reader/writer/topic and do a lookup of the type in its local type meta-data administration, which is domain scoped hash table (`ddsi_domaingv.tl_admin`).

A type lookup request can be triggered by the application, using the API function dds_domain_resolve_type or by the endpoint matching code (explained below). The API function for resolving a type takes an optional time-out, that allows the function call to wait for the requested type to become available (implemented by a condition variable `ddsi_domaingv.tl_resolved_cond`, triggered by the type lookup reply handler).

The type discovery implementation adds a set of built-in endpoints to a participant that can be used to lookup type information: the type lookup request reader/writer and the type lookup response reader/writer (see section 7.6.3.3 of the DDS XTypes specification).  A type lookup request message contains a list of type identifiers to be resolved. A node that receives the request (and has a type lookup response writer) writes a reply with the serialized type information from its own type administration. Serializing and deserializing a sertype is also part of the sertype ops interface, using the serialize and deserialize functions. For ddsi_sertype_default (the only sertype implementation that currently supports this) the generic plist serializer is used using a predefined set of ops for serializing the sertype.

Note: In the current implementation a type lookup request is sent to all nodes and any node that reads this request writes a type lookup reply message (in which the set of reply types can be empty if none of the request type ids are known in that node). This may be optimized in a future release, sending the request only to the node that has sent a type identifier in one of its SEDP messages.

## QoS Matching

The type discovery implementation introduces a number of additional checks in QoS matching in DDSI (function qos_match_mask_p). After the check for matching topic name and type name for reader and writer, the matching function checks if the type information is resolved. In case any of the types (typically from the proxy endpoint) is not resolved, matching is delayed and a type lookup request for that type identifier is sent.

An incoming type lookup reply triggers the domain scoped tl_resolved_cond condition variable (so that the type lookup API function can check if the requested type is resolved) and triggers the endpoint matching for all proxy endpoints that are using one of the resolved types. This list of proxy endpoints is retrieved from the type lookup meta-data administration (`ddsi_domaingv.tl_admin`). For each of these proxy endpoints the function update_proxy_endpoint_matching is called, which tries to connect the proxy endpoint to local endpoints.


