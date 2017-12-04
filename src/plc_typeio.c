/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */


/* Greenplum headers */
#include "postgres.h"
#include "fmgr.h"
#include "access/transam.h"
#include "access/tupmacs.h"
#include "executor/spi.h"
#include "parser/parse_type.h"
#include "utils/fmgroids.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

#include "plc_typeio.h"
#include "common/comm_utils.h"
#include "message_fns.h"
#include "plcontainer_common.h"

static void fill_type_info_inner(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type,
                                 bool isArrayElement, bool isUDTElement);

static char *plc_datum_as_int1(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int2(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int4(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int8(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float4(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float8(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float8_numeric(Datum input, plcTypeInfo *type);

static char *plc_datum_as_text(Datum input, plcTypeInfo *type);

static char *plc_datum_as_bytea(Datum input, plcTypeInfo *type);

static char *plc_datum_as_array(Datum input, plcTypeInfo *type);

static void plc_backend_array_free(plcIterator *iter);

static rawdata *plc_backend_array_next(plcIterator *self);

static char *plc_datum_as_udt(Datum input, plcTypeInfo *type);

static Datum plc_datum_from_int1(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int2(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int4(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int8(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float4(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float8(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float8_numeric(char *input, plcTypeInfo *type);

static Datum plc_datum_from_text(char *input, plcTypeInfo *type);

static Datum plc_datum_from_text_ptr(char *input, plcTypeInfo *type);

static Datum plc_datum_from_bytea(char *input, plcTypeInfo *type);

static Datum plc_datum_from_bytea_ptr(char *input, plcTypeInfo *type);

static Datum plc_datum_from_array(char *input, plcTypeInfo *type);

static Datum plc_datum_from_udt(char *input, plcTypeInfo *type);

static Datum plc_datum_from_udt_ptr(char *input, plcTypeInfo *type);

static void
fill_type_info_inner(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type, bool isArrayElement, bool isUDTElement) {
	HeapTuple typeTup;
	Form_pg_type typeStruct;
	char dummy_delim;
	Oid typioparam;

	typeTup = SearchSysCache(TYPEOID, typeOid, 0, 0, 0);
	if (!HeapTupleIsValid(typeTup))
		elog(ERROR, "cache lookup failed for type %u", typeOid);

	typeStruct = (Form_pg_type) GETSTRUCT(typeTup);
	ReleaseSysCache(typeTup);

	type->typeOid = typeOid;
	type->output = typeStruct->typoutput;
	type->input = typeStruct->typinput;
	get_type_io_data(typeOid, IOFunc_input,
	                 &type->typlen, &type->typbyval, &type->typalign,
	                 &dummy_delim,
	                 &typioparam, &type->input);
	type->typmod = typeStruct->typtypmod;
	type->nSubTypes = 0;
	type->subTypes = NULL;
	type->typelem = typeStruct->typelem;

	type->is_rowtype = false;
	type->is_record = false;
	type->attisdropped = false;
	type->typ_relid = InvalidOid;
	type->typrel_xmin = InvalidTransactionId;
	ItemPointerSetInvalid(&type->typrel_tid);
	type->typeName = NULL;

	switch (typeOid) {
		case BOOLOID:
			type->type = PLC_DATA_INT1;
			type->outfunc = plc_datum_as_int1;
			type->infunc = plc_datum_from_int1;
			break;
		case INT2OID:
			type->type = PLC_DATA_INT2;
			type->outfunc = plc_datum_as_int2;
			type->infunc = plc_datum_from_int2;
			break;
		case INT4OID:
			type->type = PLC_DATA_INT4;
			type->outfunc = plc_datum_as_int4;
			type->infunc = plc_datum_from_int4;
			break;
		case INT8OID:
			type->type = PLC_DATA_INT8;
			type->outfunc = plc_datum_as_int8;
			type->infunc = plc_datum_from_int8;
			break;
		case FLOAT4OID:
			type->type = PLC_DATA_FLOAT4;
			type->outfunc = plc_datum_as_float4;
			type->infunc = plc_datum_from_float4;
			break;
		case FLOAT8OID:
			type->type = PLC_DATA_FLOAT8;
			type->outfunc = plc_datum_as_float8;
			type->infunc = plc_datum_from_float8;
			break;
		case NUMERICOID:
			type->type = PLC_DATA_FLOAT8;
			type->outfunc = plc_datum_as_float8_numeric;
			type->infunc = plc_datum_from_float8_numeric;
			break;
		case BYTEAOID:
			type->type = PLC_DATA_BYTEA;
			type->outfunc = plc_datum_as_bytea;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_bytea;
			} else {
				type->infunc = plc_datum_from_bytea_ptr;
			}
			break;
			/* All the other types are passed through in-out functions to translate
			 * them to text before sending and after receiving */
		default:
			type->type = PLC_DATA_TEXT;
			type->outfunc = plc_datum_as_text;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_text;
			} else {
				type->infunc = plc_datum_from_text_ptr;
			}
			break;
	}

	/* Processing arrays here */
	if (!isArrayElement && typeStruct->typelem != 0 && typeStruct->typoutput == F_ARRAY_OUT) {
		type->type = PLC_DATA_ARRAY;
		type->outfunc = plc_datum_as_array;
		type->infunc = plc_datum_from_array;
		type->nSubTypes = 1;
		type->subTypes = (plcTypeInfo *) plc_top_alloc(sizeof(plcTypeInfo));
		fill_type_info_inner(fcinfo, typeStruct->typelem, &type->subTypes[0], true, isUDTElement);
	}

	/* Processing composite types - only first level is supported */
	if (!isUDTElement) {
		TupleDesc desc;

		if (typeOid == RECORDOID) {
			if (fcinfo == NULL || get_call_result_type(fcinfo, NULL, &desc) != TYPEFUNC_COMPOSITE) {
				ereport(ERROR,
				        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					        errmsg("function returning record called in context "
						               "that cannot accept type record")));
			}
			type->is_rowtype = true;

			/* bless the record to make it known to the typcache lookup code */
			BlessTupleDesc(desc);

			/* save the freshly generated typmod */
			type->typmod = desc->tdtypmod;

			/* Indicate that this type is a record */
			type->is_record = true;
		}

		if (typeStruct->typtype == TYPTYPE_COMPOSITE) {
			desc = lookup_rowtype_tupdesc(type->typeOid, type->typmod);
			type->is_rowtype = true;
		}

		if (type->is_rowtype) {
			int i;

			type->type = PLC_DATA_UDT;
			type->outfunc = plc_datum_as_udt;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_udt;
			} else {
				type->infunc = plc_datum_from_udt_ptr;
			}
			type->nSubTypes = desc->natts;

			if (desc->tdtypeid != RECORDOID && !TransactionIdIsValid(type->typrel_xmin)) {
				HeapTuple relTup;

				/* Get the pg_class tuple corresponding to the type of the input */
				type->typ_relid = typeidTypeRelid(desc->tdtypeid);
				relTup = SearchSysCache1(RELOID, ObjectIdGetDatum(type->typ_relid));
				if (!HeapTupleIsValid(relTup)) {
					elog(ERROR, "cache lookup failed for relation %u", type->typ_relid);
				}

				/* Extract the XMIN value to later use it in PLy_procedure_valid */
				type->typrel_xmin = HeapTupleHeaderGetXmin(relTup->t_data);
				type->typrel_tid = relTup->t_self;
				type->typeName = plc_top_strdup(NameStr(typeStruct->typname));

				ReleaseSysCache(relTup);
			}

			// Allocate memory for this number of arguments
			type->subTypes = (plcTypeInfo *) plc_top_alloc(type->nSubTypes * sizeof(plcTypeInfo));
			memset(type->subTypes, 0, type->nSubTypes * sizeof(plcTypeInfo));

			// Fill all the subtypes
			for (i = 0; i < desc->natts; i++) {
				type->subTypes[i].attisdropped = desc->attrs[i]->attisdropped;
				if (!type->subTypes[i].attisdropped) {
					/* We support the case with array of UDTs, each of which contains another array */
					fill_type_info_inner(fcinfo, desc->attrs[i]->atttypid, &type->subTypes[i], false, true);
				}
				type->subTypes[i].typeName = plc_top_strdup(NameStr(desc->attrs[i]->attname));
			}

			ReleaseTupleDesc(desc);
		}
	}
}

void fill_type_info(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type) {
	fill_type_info_inner(fcinfo, typeOid, type, false, false);
}

void copy_type_info(plcType *type, plcTypeInfo *ptype) {
	type->type = ptype->type;
	type->nSubTypes = 0;
	if (ptype->typeName != NULL) {
		type->typeName = pstrdup(ptype->typeName);
	} else {
		type->typeName = NULL;
	}
	if (ptype->nSubTypes > 0) {
		int i, j;

		for (i = 0; i < ptype->nSubTypes; i++) {
			if (!ptype->subTypes[i].attisdropped) {
				type->nSubTypes += 1;
			}
		}

		type->subTypes = (plcType *) pmalloc(type->nSubTypes * sizeof(plcType));
		for (i = 0, j = 0; i < ptype->nSubTypes; i++) {
			if (!ptype->subTypes[i].attisdropped) {
				copy_type_info(&type->subTypes[j], &ptype->subTypes[i]);
				j += 1;
			}
		}
	} else {
		type->subTypes = NULL;
	}
}

void free_type_info(plcTypeInfo *type) {
	int i = 0;

	if (type->typeName != NULL) {
		pfree(type->typeName);
	}

	for (i = 0; i < type->nSubTypes; i++) {
		free_type_info(&type->subTypes[i]);
	}

	if (type->nSubTypes > 0) {
		pfree(type->subTypes);
	}
}

static char *plc_datum_as_int1(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(1);
	*((char *) out) = DatumGetBool(input);
	return out;
}

static char *plc_datum_as_int2(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(2);
	*((int16 *) out) = DatumGetInt16(input);
	return out;
}

static char *plc_datum_as_int4(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(4);
	*((int32 *) out) = DatumGetInt32(input);
	return out;
}

static char *plc_datum_as_int8(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(8);
	*((int64 *) out) = DatumGetInt64(input);
	return out;
}

static char *plc_datum_as_float4(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(4);
	*((float4 *) out) = DatumGetFloat4(input);
	return out;
}

static char *plc_datum_as_float8(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(8);
	*((float8 *) out) = DatumGetFloat8(input);
	return out;
}

static char *plc_datum_as_float8_numeric(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) pmalloc(8);
	/* Numeric is casted to float8 which causes precision lost */
	Datum fdatum = DirectFunctionCall1(numeric_float8, input);
	*((float8 *) out) = DatumGetFloat8(fdatum);
	return out;
}

static char *plc_datum_as_text(Datum input, plcTypeInfo *type) {
	return DatumGetCString(OidFunctionCall3(type->output,
	                                        input,
	                                        type->typelem,
	                                        type->typmod));
}

static char *plc_datum_as_bytea(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	text *txt = DatumGetByteaP(input);
	int len = VARSIZE(txt) - VARHDRSZ;
	char *out = (char *) pmalloc(len + 4);
	*((int *) out) = len;
	memcpy(out + 4, VARDATA(txt), len);
	return out;
}

static char *plc_datum_as_array(Datum input, plcTypeInfo *type) {
	ArrayType *array = DatumGetArrayTypeP(input);
	plcIterator *iter;
	plcArrayMeta *meta;
	plcPgArrayPosition *pos;
	int i;

	iter = (plcIterator *) palloc(sizeof(plcIterator));
	meta = (plcArrayMeta *) palloc(sizeof(plcArrayMeta));
	pos = (plcPgArrayPosition *) palloc(sizeof(plcPgArrayPosition));
	iter->meta = meta;
	iter->position = (char *) pos;

	meta->type = type->subTypes[0].type;
	meta->ndims = ARR_NDIM(array);
	meta->dims = (int *) palloc(meta->ndims * sizeof(int));
	pos->type = type;
	pos->bitmap = ARR_NULLBITMAP(array);
	pos->bitmask = 1;
	meta->size = meta->ndims > 0 ? 1 : 0;
	for (i = 0; i < meta->ndims; i++) {
		meta->dims[i] = ARR_DIMS(array)[i];
		meta->size *= ARR_DIMS(array)[i];
	}
	iter->data = ARR_DATA_PTR(array);
	iter->next = plc_backend_array_next;
	iter->cleanup = plc_backend_array_free;

	return (char *) iter;
}

static void plc_backend_array_free(plcIterator *iter) {
	plcArrayMeta *meta;
	meta = (plcArrayMeta *) iter->meta;
	if (meta->ndims > 0) {
		pfree(meta->dims);
	}
	pfree(iter->meta);
	pfree(iter->position);
	return;
}

static rawdata *plc_backend_array_next(plcIterator *self) {
	plcTypeInfo *subtyp;
	rawdata *res;
	plcPgArrayPosition *pos;
	Datum itemvalue;

	res = palloc(sizeof(rawdata));
	pos = (plcPgArrayPosition *) self->position;
	subtyp = &pos->type->subTypes[0];

	/* Get source element, checking for NULL */
	if (pos->bitmap && (*(pos->bitmap) & pos->bitmask) == 0) {
		res->isnull = 1;
		res->value = NULL;
	} else {
		res->isnull = 0;
		itemvalue = fetch_att(self->data, subtyp->typbyval, subtyp->typlen);
		res->value = subtyp->outfunc(itemvalue, subtyp);

		self->data = att_addlength_pointer(self->data, subtyp->typlen, self->data);
		self->data = (char *) att_align_nominal(self->data, subtyp->typalign);
	}

	/* advance bitmap pointer if any */
	if (pos->bitmap) {
		pos->bitmask <<= 1;
		if (pos->bitmask == 0x100 /* (1<<8) */) {
			pos->bitmap++;
			pos->bitmask = 1;
		}
	}

	return res;
}

/*
HeapTupleData rec_data;
rec_data.t_len = HeapTupleHeaderGetDatumLength(rec_header);
ItemPointerSetInvalid(&(rec_data.t_self));
rec_data.t_tableOid = InvalidOid;
rec_data.t_data = rec_header;
vattr = heap_getattr(&rec_data, (i + 1), type->tupleDesc, &isnull);
*/
static char *plc_datum_as_udt(Datum input, plcTypeInfo *type) {
	HeapTupleHeader rec_header;
	plcUDT *res;
	int i, j;
	int nNonDropped = 0;

	for (i = 0; i < type->nSubTypes; i++) {
		if (!type->subTypes[i].attisdropped) {
			nNonDropped += 1;
		}
	}

	res = plc_alloc_udt(nNonDropped);

	rec_header = DatumGetHeapTupleHeader(input);
	for (i = 0, j = 0; i < type->nSubTypes; i++) {
		Datum vattr;
		bool isnull;

		if (!type->subTypes[i].attisdropped) {
			vattr = GetAttributeByNum(rec_header, (i + 1), &isnull);
			if (isnull) {
				res->data[j].isnull = true;
				res->data[j].value = NULL;
			} else {
				res->data[j].isnull = false;
				res->data[j].value = type->subTypes[i].outfunc(vattr, &type->subTypes[i]);
			}
			j++;
		}
	}

	return (char *) res;
}

static Datum plc_datum_from_int1(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return BoolGetDatum(*((bool *) input));
}

static Datum plc_datum_from_int2(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int16GetDatum(*((int16 *) input));
}

static Datum plc_datum_from_int4(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int32GetDatum(*((int32 *) input));
}

static Datum plc_datum_from_int8(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int64GetDatum(*((int64 *) input));
}

static Datum plc_datum_from_float4(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Float4GetDatum(*((float4 *) input));
}

static Datum plc_datum_from_float8(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Float8GetDatum(*((float8 *) input));
}

static Datum plc_datum_from_float8_numeric(char *input, pg_attribute_unused() plcTypeInfo *type) {
	Datum fdatum = Float8GetDatum(*((float8 *) input));
	return DirectFunctionCall1(float8_numeric, fdatum);
}

static Datum plc_datum_from_text(char *input, plcTypeInfo *type) {
	return OidFunctionCall3(type->input,
	                        CStringGetDatum(input),
	                        type->typelem,
	                        type->typmod);
}

static Datum plc_datum_from_text_ptr(char *input, plcTypeInfo *type) {
	return OidFunctionCall3(type->input,
	                        CStringGetDatum(*((char **) input)),
	                        type->typelem,
	                        type->typmod);
}

static Datum plc_datum_from_bytea(char *input, pg_attribute_unused() plcTypeInfo *type) {
	int size = *((int *) input);
	bytea *result = palloc(size + VARHDRSZ);

	SET_VARSIZE(result, size + VARHDRSZ);
	memcpy(VARDATA(result), input + 4, size);
	return PointerGetDatum(result);
}

static Datum plc_datum_from_bytea_ptr(char *input, plcTypeInfo *type) {
	return plc_datum_from_bytea(*((char **) input), type);
}

static Datum plc_datum_from_array(char *input, plcTypeInfo *type) {
	Datum dvalue;
	Datum *elems;
	ArrayType *array = NULL;
	int *lbs = NULL;
	int i;
	MemoryContext oldContext;
	plcArray *arr;
	char *ptr;
	int len;
	plcTypeInfo *subType;

	arr = (plcArray *) input;
	subType = &type->subTypes[0];
	lbs = (int *) palloc(arr->meta->ndims * sizeof(int));
	for (i = 0; i < arr->meta->ndims; i++)
		lbs[i] = 1;

	elems = palloc(arr->meta->size * sizeof(Datum));
	ptr = arr->data;
	len = plc_get_type_length(subType->type);
	for (i = 0; i < arr->meta->size; i++) {
		if (arr->nulls[i] == 0) {
			elems[i] = subType->infunc(ptr, subType);
		} else {
			elems[i] = (Datum) 0;
		}
		ptr += len;
	}

	oldContext = MemoryContextSwitchTo(pl_container_caller_context);
	array = construct_md_array(elems,
	                           arr->nulls,
	                           arr->meta->ndims,
	                           arr->meta->dims,
	                           lbs,
	                           subType->typeOid,
	                           subType->typlen,
	                           subType->typbyval,
	                           subType->typalign);

	dvalue = PointerGetDatum(array);
	MemoryContextSwitchTo(oldContext);

	pfree(lbs);
	pfree(elems);

	return dvalue;
}

static Datum plc_datum_from_udt(char *input, plcTypeInfo *type) {
	TupleDesc desc;
	HeapTuple tuple;
	Datum *values;
	bool *nulls;
	volatile int i, j;
	MemoryContext oldContext;
	plcUDT *udt = (plcUDT *) input;

	/* Build tuple */
	values = palloc(sizeof(Datum) * type->nSubTypes);
	nulls = palloc(sizeof(bool) * type->nSubTypes);
	for (i = 0, j = 0; i < type->nSubTypes; ++i) {
		if (!type->subTypes[i].attisdropped) {
			if (udt->data[j].isnull) {
				nulls[i] = true;
				values[i] = (Datum) 0;
			} else {
				nulls[i] = false;
				values[i] = type->subTypes[j].infunc(udt->data[j].value, &type->subTypes[j]);
			}
			j += 1;
		}
	}

	oldContext = MemoryContextSwitchTo(pl_container_caller_context);
	desc = lookup_rowtype_tupdesc(type->typeOid, type->typmod);
	tuple = heap_form_tuple(desc, values, nulls);
	ReleaseTupleDesc(desc);
	MemoryContextSwitchTo(oldContext);

	pfree(values);
	pfree(nulls);

	return HeapTupleGetDatum(tuple);
}

static Datum plc_datum_from_udt_ptr(char *input, plcTypeInfo *type) {
	return plc_datum_from_udt(*((char **) input), type);
}

plcDatatype plc_get_datatype_from_oid(Oid oid) {
	plcDatatype dt;

	switch (oid) {
		case BOOLOID:
			dt = PLC_DATA_INT1;
			break;
		case INT2OID:
			dt = PLC_DATA_INT2;
			break;
		case INT4OID:
			dt = PLC_DATA_INT4;
			break;
		case INT8OID:
			dt = PLC_DATA_INT8;
			break;
		case FLOAT4OID:
			dt = PLC_DATA_FLOAT4;
			break;
		case FLOAT8OID:
			dt = PLC_DATA_FLOAT8;
			break;
		case NUMERICOID:
			dt = PLC_DATA_FLOAT8;
			break;
		case BYTEAOID:
			dt = PLC_DATA_BYTEA;
			break;
		default:
			dt = PLC_DATA_TEXT;
			break;
	}

	return dt;
}
