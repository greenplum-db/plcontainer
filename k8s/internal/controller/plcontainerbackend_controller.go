/*
Copyright 2023.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
Wapp.kubernetes.io/instanceITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package controller

import (
	"context"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	corev1 "k8s.io/api/core/v1"
	"k8s.io/client-go/util/workqueue"
	"k8s.io/utils/pointer"

	"k8s.io/apimachinery/pkg/api/resource"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/apimachinery/pkg/runtime"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/event"
	"sigs.k8s.io/controller-runtime/pkg/handler"
	"sigs.k8s.io/controller-runtime/pkg/log"
	"sigs.k8s.io/controller-runtime/pkg/manager"

	plcontainerv1 "greenplum.org/plcontainer/api/v1"
)

// PLContainerBackendReconciler reconciles a PLContainerBackend object
type PLContainerBackendReconciler struct {
	client.Client

	lock        sync.Mutex
	portallocer []map[int32]struct{} // TODO use bitmap
	Scheme      *runtime.Scheme
}

func (r *PLContainerBackendReconciler) markPortUsed(p int32) {
	r.lock.Lock()
	defer r.lock.Unlock()

	portUsedOnHost := 0

	for i := range r.portallocer {
		if _, ok := r.portallocer[i][p]; ok {
			portUsedOnHost += 1
			continue
		}

		r.portallocer[i][p] = struct{}{}
	}
}

func (r *PLContainerBackendReconciler) markPortNotUsed(p int32) {
	r.lock.Lock()
	defer r.lock.Unlock()

	removeHostIDX := -1

	for i := len(r.portallocer) - 1; i >= 0; i-- {
		if _, ok := r.portallocer[i][p]; !ok {
			continue
		}

		delete(r.portallocer[i], p)
		if len(r.portallocer[i]) == 0 {
			removeHostIDX = i
		}

		break
	}

	if removeHostIDX != -1 {
		// remove r.markPortNotUsed[removeHostIDX]
		r.portallocer = append(r.portallocer[:removeHostIDX], r.portallocer[removeHostIDX+1:]...)
	}
}

func (r *PLContainerBackendReconciler) allocPort(rangeMin int32, rangeMax int32) int32 {
	r.lock.Lock()
	defer r.lock.Unlock()

	for i := len(r.portallocer) - 1; i >= 0; i-- {
		for port := rangeMin; port <= rangeMax; port++ {
			if _, ok := r.portallocer[i][port]; !ok {
				r.portallocer[i][port] = struct{}{}
				return port
			}
		}
	}

	r.portallocer = append(r.portallocer, map[int32]struct{}{rangeMin: {}})
	return rangeMin
}

//+kubebuilder:rbac:groups=plcontainer.greenplum.org,resources=plcontainerbackends,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=plcontainer.greenplum.org,resources=plcontainerbackends/status,verbs=get;update;patch
//+kubebuilder:rbac:groups=plcontainer.greenplum.org,resources=plcontainerbackends/finalizers,verbs=update

// PLContainerBackendController need to watch list and create pods, add the permission
//+kubebuilder:rbac:groups="",resources=pods,verbs=get;list;watch;create

// Reconcile is part of the main kubernetes reconciliation loop which aims to
// move the current state of the cluster closer to the desired state.
//
// For more details, check Reconcile and its Result here:
// - https://pkg.go.dev/sigs.k8s.io/controller-runtime@v0.16.0/pkg/reconcile
func (r *PLContainerBackendReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log := log.FromContext(ctx)

	var plcBackend plcontainerv1.PLContainerBackend
	if err := r.Client.Get(ctx, req.NamespacedName, &plcBackend); err != nil {
		// user is deleting the plcBackend object
		return ctrl.Result{}, client.IgnoreNotFound(err)
	}

	if plcBackend.Status.Status != "" {
		// plcBackend is doing self-updating (in Pod callback), ignore this reconcile
		return ctrl.Result{}, nil
	}

	newPod := r.AllocNewPod(&plcBackend)

	if _, err := ctrl.CreateOrUpdate(ctx, r.Client, &newPod, func() error {
		// use this label to get backend object in pod create callback
		newPod.Labels["greenplum.org/backendname"] = plcBackend.Name

		// set this pod is referenced by plcBackend, when plcBackend is deleted, this pod will be deleted too
		return ctrl.SetControllerReference(&plcBackend, &newPod, r.Scheme)
	}); err != nil {
		log.Error(err, "failed to create or update pod", "pod", newPod)
		return ctrl.Result{}, client.IgnoreAlreadyExists(err)
	}

	log.Info("create new pod", "newpodname", newPod.Name)

	plcBackend.Status.BackendPodName = newPod.Name
	plcBackend.Status.Status = plcontainerv1.BackendStatusBotting

	if err := r.Status().Update(ctx, &plcBackend); err != nil {
		log.Error(err, "failed to update plcBackend status")
		return ctrl.Result{}, nil
	}

	return ctrl.Result{}, nil
}

// PodCreate is the event handler for pod create events
func (r *PLContainerBackendReconciler) PodCreate(c context.Context, e event.CreateEvent, pod *corev1.Pod, q workqueue.RateLimitingInterface) {
	log := log.FromContext(c)

	// sync port alloc status for all pods, and ignore the pod not created by plcBackend
	r.sync_portalloc(c, pod, HostPortAlloc)

	// if this pod is not created by plcBackend, skip
	if v, ok := pod.Labels["app.kubernetes.io/managed-by"]; !ok || v != "greenplum.org-plcontainer" {
		return
	}

	backendname, ok := pod.Labels["greenplum.org/backendname"]
	if !ok {
		return
	}

	// sync status back to plcBackend object
	var backend plcontainerv1.PLContainerBackend
	if err := r.Client.Get(c, client.ObjectKey{Namespace: pod.Namespace, Name: backendname}, &backend); err != nil {
		log.Error(err, "failed to get plcBackend object", "backendname", backendname)
		return
	}

	// set backend status to running
	backend.Status.BackendPodName = pod.Name
	backend.Status.Status = plcontainerv1.BackendStatusBotting

	log.Info("update backend status", "status", backend.Status, "backendname", backend.Name)
	if err := r.Status().Update(c, &backend); err != nil {
		log.Error(err, "failed to update plcBackend status")
	}
}

func (r *PLContainerBackendReconciler) PodUpdate(c context.Context, e event.UpdateEvent, old *corev1.Pod, new *corev1.Pod, w workqueue.RateLimitingInterface) {
	log := log.FromContext(c)

	// sync host port changes
	{
		port_is_eq := true

		collect_hostport := func(p []int, cs []corev1.Container) []int {
			for i := range cs {
				for j := range cs[i].Ports {
					if cs[i].Ports[j].HostPort != 0 {
						p = append(p, int(cs[i].Ports[j].HostPort))
					}
				}
			}
			return p
		}

		if len(old.Spec.Containers) != len(new.Spec.Containers) {
			port_is_eq = false
		} else {
			oldport := collect_hostport(nil, old.Spec.Containers)
			newport := collect_hostport(nil, new.Spec.Containers)

			if len(oldport) != len(newport) {
				port_is_eq = false
			} else {
				sort.Ints(oldport)
				sort.Ints(newport)

				for i := range oldport {
					if oldport[i] != newport[i] {
						port_is_eq = false
						break
					}
				}
			}
		}

		if !port_is_eq {
			r.sync_portalloc(c, old, HostPortRelease)
			r.sync_portalloc(c, new, HostPortAlloc)
		}
	}

	// check if is the pod for plcBackend. and the status is set tp Complete
	if v, ok := new.Labels["app.kubernetes.io/managed-by"]; ok && v == "greenplum.org-plcontainer" {
		backendname, ok := new.Labels["greenplum.org/backendname"]
		if !ok {
			return
		}

		var backend plcontainerv1.PLContainerBackend
		if err := r.Client.Get(c, client.ObjectKey{Namespace: new.Namespace, Name: backendname}, &backend); err != nil {
			return
		}

		// to PodRunning. set the backend to running
		if new.Status.Phase == corev1.PodRunning {
			backend.Status.Status = plcontainerv1.BackendStatusRunnig

			switch backend.Spec.NetworkMode {
			case plcontainerv1.NetworkModeHostPort:
				backend.Status.BackendIP = new.Spec.Containers[0].Ports[0].HostIP
				backend.Status.BackendPort = new.Spec.Containers[0].Ports[0].HostPort
			case plcontainerv1.NetworkModeClusterIP:
				backend.Status.BackendIP = new.Status.PodIP
				backend.Status.BackendPort = new.Spec.Containers[0].Ports[0].ContainerPort
			}

			r.Status().Update(c, &backend)
		}

		// to PodSucceeded. delete the plcBackend
		if new.Status.Phase == corev1.PodSucceeded {
			log.Info("delete plcontainer backend because pod is finished", "backendname", backend.Name)

			backend.Status.Status = plcontainerv1.BackendStatusCompleted
			r.Status().Update(c, &backend)

			r.Client.Delete(c, &backend)
		}

		// to PodFailed. also delete the plcBackend but wait some time
		if new.Status.Phase == corev1.PodFailed {
			log.Info("delete plcontainer backend because pod is failed", "backendname", backend.Name)

			backend.Status.Status = plcontainerv1.BackendStatusCompleted
			r.Status().Update(c, &backend)

			go func() {
				time.Sleep(time.Second * 60)
				r.Client.Delete(c, &backend)
			}()
		}
	}
}

func (r *PLContainerBackendReconciler) PodDelete(c context.Context, e event.DeleteEvent, pod *corev1.Pod, w workqueue.RateLimitingInterface) {
	r.sync_portalloc(c, pod, HostPortRelease)
}

var _ handler.EventHandler = &PLContainerBackendReconciler{}

func (r *PLContainerBackendReconciler) Create(c context.Context, e event.CreateEvent, w workqueue.RateLimitingInterface) {
	switch e.Object.(type) {
	case *corev1.Pod:
		r.PodCreate(c, e, e.Object.(*corev1.Pod), w)
	}
}

func (r *PLContainerBackendReconciler) Update(c context.Context, e event.UpdateEvent, w workqueue.RateLimitingInterface) {
	switch e.ObjectNew.(type) {
	case *corev1.Pod:
		r.PodUpdate(c, e, e.ObjectOld.(*corev1.Pod), e.ObjectNew.(*corev1.Pod), w)
	}
}

func (r *PLContainerBackendReconciler) Delete(c context.Context, e event.DeleteEvent, w workqueue.RateLimitingInterface) {
	switch e.Object.(type) {
	case *corev1.Pod:
		r.PodDelete(c, e, e.Object.(*corev1.Pod), w)
	}
}

func (r *PLContainerBackendReconciler) Generic(context.Context, event.GenericEvent, workqueue.RateLimitingInterface) {
}

type HostPortAction int32

const (
	HostPortAlloc HostPortAction = iota
	HostPortRelease
)

func (r *PLContainerBackendReconciler) sync_portalloc(c context.Context, pod *corev1.Pod, action HostPortAction) {
	for _, container := range pod.Spec.Containers {
		for _, port := range container.Ports {
			hostport := port.HostPort

			if hostport == 0 {
				// container dose not alloc a hostport
				continue
			}

			switch action {
			case HostPortAlloc:
				r.markPortUsed(hostport)
			case HostPortRelease:
				r.markPortNotUsed(hostport)
			}
		}
	}
}

// https://kubernetes.io/docs/concepts/overview/working-with-objects/common-labels/
func getPodLabesTemplate(instance string) map[string]string {
	r := map[string]string{
		"app.kubernetes.io/name":       "plcontainerbackend",
		"app.kubernetes.io/managed-by": "greenplum.org-plcontainer",
		"app.kubernetes.io/version":    "5.7.21",
		"app.kubernetes.io/component":  "database",
		"app.kubernetes.io/part-of":    "greenplum.org-plcontainer",
	}

	if instance != "" {
		r["app.kubernetes.io/instance"] = instance
	}

	return r
}

func (r *PLContainerBackendReconciler) AllocNewPod(spec *plcontainerv1.PLContainerBackend) corev1.Pod {
	ret := corev1.Pod{}

	ret.Namespace = spec.Namespace
	ret.GenerateName = spec.Name + "-"
	ret.Labels = getPodLabesTemplate(spec.Spec.InstanceID)
	ret.Spec.RestartPolicy = corev1.RestartPolicyNever

	// more label
	{
		ret.Labels["greenplum.org/segindex"] = strconv.Itoa(int(spec.Spec.Segindex))
		ret.Labels["greenplum.org/owner"] = spec.Spec.UserName
		ret.Labels["greenplum.org/databaseid"] = strconv.Itoa(int(spec.Spec.DBID))
		ret.Labels["greenplum.org/databasename"] = spec.Spec.DatabaseName
	}

	// inject plcontainer Client
	pod_volum := []corev1.Volume{}
	container_volumMount := []corev1.VolumeMount{}
	{
		switch spec.Spec.SetupMethod {
		case "":
			fallthrough
		case plcontainerv1.SetupMethodClientInHostpath:
			// setup in HostVolume part
		case plcontainerv1.SetupMethodClientInContainer:
			// not need to setup
		case plcontainerv1.SetupMethodClientInInitContainer:
			v := corev1.Volume{}
			v.Name = "plcontainer_client"
			v.EmptyDir = &corev1.EmptyDirVolumeSource{}
			pod_volum = append(pod_volum, v)

			m := corev1.VolumeMount{}
			m.Name = v.Name
			m.MountPath = "/clientdir"
			container_volumMount = append(container_volumMount, m)

			init := corev1.Container{
				Name:         "plcontainer-init",
				Image:        "",
				Command:      []string{"true"}, // TODO
				Args:         []string{},       // TODO
				VolumeMounts: []corev1.VolumeMount{m},
			}
			ret.Spec.InitContainers = append(ret.Spec.InitContainers, init)
		}
	}

	// volume mount
	{
		hosts, containers, readonlys, err := plcontainerv1.ParseHostVolume(spec.Spec.HostVolume)
		if err != nil {
			log.Log.Error(err, "failed to render pod template")
		}

		for i := range hosts {
			host := hosts[i]
			container := containers[i]
			readonly := readonlys[i]

			name := strings.ReplaceAll(host, "/", "_")

			v := corev1.Volume{
				Name: name,
				VolumeSource: corev1.VolumeSource{HostPath: &corev1.HostPathVolumeSource{
					Path: host,
					Type: (*corev1.HostPathType)(pointer.String(string(corev1.HostPathDirectory))),
				}},
			}
			pod_volum = append(pod_volum, v)

			m := corev1.VolumeMount{
				Name:      name,
				MountPath: container,
				ReadOnly:  readonly,
			}
			container_volumMount = append(container_volumMount, m)
		}

		ret.Spec.Volumes = append(ret.Spec.Volumes, pod_volum...)
	}

	// core container part, cmdline, env, res and networking
	{
		env := []corev1.EnvVar{
			{Name: "EXECUTOR_UID", Value: strconv.Itoa(int(spec.Spec.Uid))},
			{Name: "EXECUTOR_GID", Value: strconv.Itoa(int(spec.Spec.Gid))},
			{Name: "DB_QE_PID", Value: strconv.Itoa(int(spec.Spec.QEpid))},
			{Name: "DB_NAME", Value: spec.Spec.DatabaseName},
			{Name: "DB_USER_NAME", Value: spec.Spec.UserName},
			{Name: "USE_CONTAINER_NETWORK", Value: "true"}, // in k8s, always using network
		}

		res := corev1.ResourceRequirements{Limits: make(corev1.ResourceList, 2)}
		if spec.Spec.CpuShare != 0 {
			cpu := int64(spec.Spec.CpuShare) // mcore to core
			res.Limits[corev1.ResourceCPU] = *resource.NewQuantity(cpu, resource.DecimalExponent)
		}
		if spec.Spec.MemoryMB != 0 {
			mem := int64(spec.Spec.MemoryMB) * 1024 * 1024
			res.Limits[corev1.ResourceMemory] = *resource.NewQuantity(mem, resource.DecimalExponent)
		}

		container := corev1.Container{
			Name:            "plcontainer-backend",
			Image:           spec.Spec.Image,
			Command:         []string{spec.Spec.Command},
			Args:            spec.Spec.Args,
			Ports:           []corev1.ContainerPort{{Name: "plc-port", ContainerPort: 8080, Protocol: "TCP"}},
			ImagePullPolicy: corev1.PullIfNotPresent,
			Env:             env,
			Resources:       res,
		}

		if spec.Spec.NetworkMode == plcontainerv1.NetworkModeHostPort {
			container.Ports[0].HostPort = r.allocPort(spec.Spec.PortRange.Min, spec.Spec.PortRange.Max)
		}

		container.VolumeMounts = append(container.VolumeMounts, container_volumMount...)

		ret.Spec.Containers = []corev1.Container{container}
	}

	return ret
}

// SetupWithManager sets up the controller with the Manager.
func (r *PLContainerBackendReconciler) SetupWithManager(mgr ctrl.Manager) error {
	log := mgr.GetLogger()
	mgr.Add(manager.RunnableFunc(func(ctx context.Context) error {
		pods := corev1.PodList{}
		c := mgr.GetClient()
		globalNS := client.ListOptions{Namespace: ""}
		plcLables := client.ListOptions{LabelSelector: labels.SelectorFromValidatedSet(getPodLabesTemplate(""))}
		if err := c.List(ctx, &pods, &globalNS, &plcLables); err != nil {
			log.Error(err, "failed to list all pod")
			return err
		}

		for i := range pods.Items {
			pod := pods.Items[i]
			r.sync_portalloc(ctx, &pod, HostPortAlloc)
		}

		backends := plcontainerv1.PLContainerBackendList{}
		if err := c.List(ctx, &backends, &globalNS); err != nil {
			log.Error(err, "failed to clean up old backend")
		}

		for i := range backends.Items {
			backend := backends.Items[i]
			if backend.Status.Status == plcontainerv1.BackendStatusCompleted {
				log.Info("clean up completed backend", "backendname", backend.Name)
				c.Delete(ctx, &backend)
			}
		}

		return nil
	}))

	return ctrl.NewControllerManagedBy(mgr).
		For(&plcontainerv1.PLContainerBackend{}).
		Watches(&corev1.Pod{}, r).
		Complete(r)
}
