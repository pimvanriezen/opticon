#include <libopticon/var.h>
#include <libopticon/var_dump.h>
#include <libopticon/util.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main (int argc, const char *argv[]) {
    const char *tstr = NULL;
    var *env = var_alloc();
    var *env_collector = var_get_dict_forkey (env, "collector");
    var *env_colors = var_get_array_forkey (env, "colors");
    var_set_int_forkey (env_collector, "listenport", 3333);
    var_set_str_forkey (env_collector, "address", "127.0.0.1");
    var_set_str_forkey (env_collector, "key", "secret");
    var_clear_array (env_colors);
    var_add_str (env_colors, "red");
    var_add_str (env_colors, "green");
    var_add_str (env_colors, "blue");
    
    assert (var_contains_str (env_colors, "green"));
    assert (! var_contains_str (env_colors, "purple"));
    assert (var_indexof (env_colors, "blue") == 2);
    assert (var_get_int_forkey (env_collector, "listenport") == 3333);
    assert (tstr = var_get_str_forkey (env_collector, "address"));
    assert (strcmp (tstr, "127.0.0.1") == 0);
    assert (tstr = var_get_str_forkey (env_collector, "key"));
    assert (strcmp (tstr, "secret") == 0);
    assert (var_get_count (env_collector) == 3);
    assert (tstr = var_get_str_atindex (env_colors, 0));
    assert (strcmp (tstr, "red") == 0);
    assert (tstr = var_get_str_atindex (env_colors, 1));
    assert (strcmp (tstr, "green") == 0);
    assert (tstr = var_get_str_atindex (env_colors, 2));
    assert (strcmp (tstr, "blue") == 0);
    
    time_t tnow = time (NULL);
    var_set_time_forkey (env, "nowtime", tnow);
    assert (var_get_time_forkey (env, "nowtime") == tnow);
    var_set_unixtime_forkey (env, "unixtime", tnow);
    assert (var_get_time_forkey (env, "unixtime") == tnow);
    
    uuid uuid_in = uuidgen();
    var_set_uuid_forkey (env, "myuuid", uuid_in);
    uuid uuid_out = var_get_uuid_forkey (env, "myuuid");
    assert (uuidcmp (uuid_in, uuid_out));
    
    var_new_generation (env);
    var_set_int_forkey (env_collector, "listenport", 3333);
    var_set_str_forkey (env_collector, "address", "192.168.1.1");
    var_clear_array (env_colors);
    var_add_str (env_colors, "red");
    var_add_str (env_colors, "green");
    var_add_str (env_colors, "blue");
    
    var *lport, *addr, *key;
    lport = var_find_key (env_collector, "listenport");
    addr = var_find_key (env_collector, "address");
    key = var_find_key (env_collector, "key");
    assert (lport);
    assert (addr);
    assert (key);
    
    /* collector.listenport should be unchanged from first generation */
    assert (lport->generation == env->generation);
    assert (lport->lastmodified < env->generation);
    assert (lport->firstseen == lport->lastmodified);
    
    /* collector.addr should be changed this generation */
    assert (addr->generation == env->generation);
    assert (addr->lastmodified == env->generation);
    assert (addr->firstseen == env->firstseen);
    
    /* colors should be changed this generation */
    assert (env_colors->generation == env->generation);
    assert (env_colors->lastmodified == env->generation);
    assert (env_colors->firstseen == env->firstseen);
    
    /* collector.key should be stale */
    assert (key->generation < env->generation);
    
    var_clean_generation (env);
    
    /* collector.key should now be deleted */
    key = var_find_key (env_collector, "key");
    assert (key == NULL);
    
    /* test var_clone */
    var *vorig = var_alloc();
    var_set_int_forkey (vorig, "test", 42);
    var_set_str_forkey (vorig, "foo", "bar");
    
    var *vcopy = var_clone (vorig);
    assert (vcopy->type == VAR_DICT);
    assert (vcopy->value.arr.count == 2);
    assert (var_find_key (vcopy, "foo"));
    assert (var_get_int_forkey (vcopy, "test") == 42);
    var_free (vcopy);
    assert (vorig->type == VAR_DICT);
    assert (vorig->value.arr.count == 2);
    assert (var_find_key (vorig, "foo"));
    assert (var_get_int_forkey (vorig, "test") == 42);
    var_free (vorig);
    
    /* test var_merge */
    var *oparent = var_alloc();
    var *onode1 = var_get_dict_forkey (oparent, "test");
    var *onode2 = var_get_array_forkey (onode1, "skip");
    var_add_str (onode2, "eth0");
    var *omerge = var_alloc();
    var *onode3 = var_get_dict_forkey (omerge, "test");
    var_set_int_forkey (onode3, "level", 42);
    var_merge (oparent, omerge);
    var_free (omerge);
    
    assert (var_get_dict_forkey (oparent, "test") == onode1);
    assert (var_get_int_forkey (onode1, "level") == 42);
    assert (var_get_array_forkey (onode1, "skip") == onode2);
    assert (var_get_count (onode2) == 1);
    assert (strcmp (var_get_str_atindex (onode2, 0), "eth0") == 0);
    
    var_free (oparent);
    
    /* test var sorting */
    var *unsorted = var_alloc();
    var *row = var_get_dict_forkey (unsorted, "0001");
    var_set_int_forkey (row, "weight",10);
    var_set_int_forkey (row, "quality",10);
    var_set_str_forkey (row, "variation","b");
    row = var_get_dict_forkey (unsorted, "0002");
    var_set_int_forkey (row, "weight",10);
    var_set_int_forkey (row, "quality",10);
    var_set_str_forkey (row, "variation","a");
    row = var_get_dict_forkey (unsorted, "0003");
    var_set_int_forkey (row, "weight",10);
    var_set_int_forkey (row, "quality",20);
    var_set_str_forkey (row, "variation","a");
    row = var_get_dict_forkey (unsorted, "0004");
    var_set_int_forkey (row, "weight",5);
    var_set_int_forkey (row, "quality",20);
    row = var_get_dict_forkey (unsorted, "0005");
    var_set_int_forkey (row, "weight",5);
    var_set_int_forkey (row, "quality",10);
    var_set_str_forkey (row, "variation","a");
 
    var *sorted = var_sort_keys (unsorted, SORT_ASCEND, "weight", "quality",
                                 "variation");
    
    row = var_first (sorted);
    assert (row);
    assert (var_get_int_forkey (row, "weight") == 5);
    assert (var_get_int_forkey (row, "quality") == 10);
    row = row->next;
    assert (row);
    assert (var_get_int_forkey (row, "weight") == 5);
    assert (var_get_int_forkey (row, "quality") == 20);
    row = row->next;
    assert (row);
    assert (var_get_int_forkey (row, "weight") == 10);
    assert (var_get_int_forkey (row, "quality") == 10);
    assert (strcmp (var_get_str_forkey (row, "variation"), "a") == 0);
    row = row->next;
    assert (row);
    assert (var_get_int_forkey (row, "weight") == 10);
    assert (var_get_int_forkey (row, "quality") == 10);
    assert (strcmp (var_get_str_forkey (row, "variation"), "b") == 0);
    row = row->next;
    assert (row);
    assert (var_get_int_forkey (row, "weight") == 10);
    assert (var_get_int_forkey (row, "quality") == 20);

    var_free (sorted);
    return 0;
}
