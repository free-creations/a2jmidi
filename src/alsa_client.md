# Naming rules for ALSA sequencer clients
How does ALSA allocate the names for clients?

Lets have a look into ALSA source code on
[git](https://github.com/alsa-project/alsa-lib).

In file 
[seqmid.c](https://github.com/alsa-project/alsa-lib/blob/3ec6dce5198f100fa8dd2abfc1258fa4138ceb1a/src/seq/seqmid.c)
we have:
```c
int snd_seq_set_client_name(snd_seq_t *seq, const char *name)
{
	snd_seq_client_info_t info;
	int err;

	if ((err = snd_seq_get_client_info(seq, &info)) < 0)
		return err;
	strncpy(info.name, name, sizeof(info.name) - 1);
	return snd_seq_set_client_info(seq, &info);
}
```
Wer see, apart from truncating the name to `sizeof(info.name) - 1` characters 
(which is 63 characters on my installation), ALSA seems not to do any verifications.  

Lets have a look what happens when we try to allocate really strange names (we use client nr 128):

- ' ' leads to 'Client-128'. Blank names are converted to something "Client-"+ client-number.
- 'aa:bb' leads to 'aa:bb'. Thus a name that suggests to be a port id is not rejected.
- 'Client-127' leads to 'Client-127'. Thus a name that suggests to be an other client is not rejected.
- Trying to use the string "130" as client name, succeeds. In consequence we might have a client whose 
Client Number is equal to 128 and whose name is "130". Really confusing...