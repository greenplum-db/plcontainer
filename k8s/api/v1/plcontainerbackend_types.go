/*
Copyright 2023.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package v1

import (
	"errors"
	"strings"

	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// EDIT THIS FILE!  THIS IS SCAFFOLDING FOR YOU TO OWN!
// NOTE: json tags are required.  Any new fields you add must have json tags for the fields to be serialized.

type PLContainerBackendPortRange struct {
	Min int32 `json:"min"`
	Max int32 `json:"max"`
}

const (
	SetupMethodClientInContainer     = "ClientInContainer"
	SetupMethodClientInInitContainer = "ClientInInitContainer"
	SetupMethodClientInHostpath      = "ClientInHostpath"

	BackendStatusBotting   = "Botting"
	BackendStatusRunnig    = "Running"
	BackendStatusCompleted = "Completed"

	NetworkModeHostPort  = "HostPort"
	NetworkModeClusterIP = "ClusterIP"
)

// PLContainerBackendSpec defines the desired state of PLContainerBackend
type PLContainerBackendSpec struct {
	// INSERT ADDITIONAL SPEC FIELDS - desired state of cluster
	// Important: Run "make" to regenerate code after modifying this file

	// InstanceID is the value for app.kubernetes.io/instance labes
	InstanceID string `json:"instanceid"`

	// SetupMethod is the method about how to start plcontainer client
	// avalive options is ["ClientInContainer", "ClientInInitContainer", "ClientInHostpath"]
	SetupMethod string `json:"setupmethod"`

	// NetworkMode is the mode to create the network connection from Greenplum to language client
	// avalive option is "HostPort" or "ClusterIP"
	// When using HostPort, will alloc a HostPort on Pod and using HostIP and HostPort to create the connection
	// When using ClusterIP, will use ClusterIP on the Pod and default Port to create the connection
	NetworkMode string `json:"networkmode"`

	// PortRange is the TCP port poll rang if using NetworkMode=HostPort
	PortRange PLContainerBackendPortRange `json:"portrange,omitempty"`

	// Command is the entry point for the continer
	Command string `json:"command"`

	// Args is the arguments for Command
	Args []string `json:"args"`

	// Uid is the user id passed from Greenplum
	Uid int32 `json:"uid"`

	// Gid is the group id passed from Greenplum
	Gid int32 `json:"gid"`

	// UserName is the user name passed from Greenplum
	DatabaseName string `json:"databasename"`

	// UserName is the user name passed from Greenplum
	UserName string `json:"username"`

	// QEpid is the Query Executer pid passed from Greenplum
	QEpid int32 `json:"qepid"`

	// DBID is the database id passed from Greenplum
	DBID int32 `json:"dbid"`

	// Segindex is the segment index passed from Greenplum
	Segindex int32 `json:"segindex"`

	// Image is the OCI image name to boot the continer
	Image string `json:"image"`

	// CpuShare is a value to set the cpu limit
	CpuShare int32 `json:"cpushare"`

	// MemoryMB is a value to set the memory limit
	MemoryMB int32 `json:"memorymb"`

	// HostVolume is the files mapped into continer using hostpath
	// format '(\/[a-zA-Z0-9_\/-]*[^\/]):(\/[a-zA-Z0-9_\/-]*[^\/]):[ro|rw]'
	//         ^^^^                        ^^^^
	//         host                        container
	HostVolume []string `json:"hostvolume"`
}

func ParseHostVolume(vs []string) ([]string, []string, []bool, error) {
	hosts := []string{}
	containers := []string{}
	readonly := []bool{}

	for i := range vs {
		v := vs[i]
		p := strings.Split(v, ":")
		if len(p) != 3 {
			return nil, nil, nil, errors.New("host volume has a format error")
		}

		hosts = append(hosts, p[0])
		containers = append(containers, p[1])
		if p[2] == "false" {
			readonly = append(readonly, false)
		} else {
			readonly = append(readonly, true)
		}
	}

	return hosts, containers, readonly, nil
}

// PLContainerBackendStatus defines the observed state of PLContainerBackend
type PLContainerBackendStatus struct {
	// INSERT ADDITIONAL STATUS FIELD - define observed state of cluster
	// Important: Run "make" to regenerate code after modifying this file

	// Status is the Status of the Backend. avalive value is ["Booting", "Running", "Completed"]
	Status string `json:"status,omitempty"`

	// BackendPodName is the name of the language client pod
	BackendPodName string `json:"podname,omitempty"`

	// BackendIP is the IP Address of the language client
	BackendIP string `json:"hostip,omitempty"`

	// BackendPort is the IP Port of the language client
	BackendPort int32 `json:"hostport,omitempty"`
}

//+kubebuilder:object:root=true
//+kubebuilder:subresource:status

// PLContainerBackend is the Schema for the plcontainerbackends API
type PLContainerBackend struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   PLContainerBackendSpec   `json:"spec,omitempty"`
	Status PLContainerBackendStatus `json:"status,omitempty"`
}

//+kubebuilder:object:root=true

// PLContainerBackendList contains a list of PLContainerBackend
type PLContainerBackendList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []PLContainerBackend `json:"items"`
}

func init() {
	SchemeBuilder.Register(&PLContainerBackend{}, &PLContainerBackendList{})
}
